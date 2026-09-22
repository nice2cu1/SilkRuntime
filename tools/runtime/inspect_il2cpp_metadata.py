"""检查 IL2CPP v31 元数据，但不假定其中包含原生地址。

表尺寸和记录布局遵循 MIT 许可的 Il2CppDumper 元数据模型（参见
tools/vendor/Il2CppDumper）。本脚本只报告元数据侧名称、所属 image/assembly
记录、方法记录和字段记录。原生指针仍然需要匹配的 main NSO，因此会有意输出为
空值。
"""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path


TARGETS = {
    "ImageConversion": {
        "namespace": "UnityEngine",
        "methods": {"LoadImage", "LoadImage_Injected"},
    },
    "Texture2D": {
        "namespace": "UnityEngine",
        "methods": {"LoadImage", "get_width", "get_height", "get_name", "LoadRawTextureData"},
    },
    "Material": {"namespace": "UnityEngine", "methods": {"get_mainTexture", "set_mainTexture"}},
    "tk2dSpriteCollectionData": {
        "namespace": "TeamCherry.TK2D",
        "methods": {"InitMaterialIds", "GetSpriteDefinition", "UnloadTextures"},
    },
}


def cstring(blob: bytes, offset: int) -> str:
    if offset < 0 or offset >= len(blob):
        return f"<invalid-string-index:{offset}>"
    end = blob.find(b"\0", offset)
    if end < 0:
        end = len(blob)
    return blob[offset:end].decode("utf-8", errors="replace")


def read_u32(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<I", blob, offset)[0]


def parse(path: Path) -> dict:
    blob = path.read_bytes()
    sanity, version = struct.unpack_from("<II", blob, 0)
    if sanity != 0xFAB11BAF:
        raise ValueError(f"元数据魔数无效：0x{sanity:08x}")
    if version != 31:
        raise ValueError(f"此证据脚本要求元数据 v31，实际为 {version}")

    # v31 头部：32 字节的魔数/版本前缀，后跟 31 个偏移/尺寸对。
    pair_names = [
        "stringLiteral", "stringLiteralData", "string", "events", "properties",
        "methods", "parameterDefaultValues", "fieldDefaultValues",
        "fieldAndParameterDefaultValueData", "fieldMarshaledSizes", "parameters",
        "fields", "genericParameters", "genericParameterConstraints",
        "genericContainers", "nestedTypes", "interfaces", "vtableMethods",
        "interfaceOffsets", "typeDefinitions", "images", "assemblies", "fieldRefs",
        "referencedAssemblies", "attributeData", "attributeDataRange",
        "unresolvedVirtualCallParameterTypes", "unresolvedVirtualCallParameterRanges",
        "windowsRuntimeTypeNames", "windowsRuntimeStrings", "exportedTypeDefinitions",
    ]
    header = {}
    cursor = 8
    for name in pair_names:
        offset, size = struct.unpack_from("<II", blob, cursor)
        header[name] = {"offset": offset, "size": size}
        cursor += 8
    if cursor != 256:
        raise AssertionError(f"v31 头部尺寸异常：{cursor}")

    strings = blob[header["string"]["offset"]:header["string"]["offset"] + header["string"]["size"]]

    # v31 image/assembly 记录由 Il2CppDumper 元数据模型定义。将此映射保留在证据
    # 报告中，可以区分类型的元数据命名空间与所属托管 image/assembly。
    image_size = 40
    assembly_size = 64
    image_defs = []
    for i in range(header["images"]["size"] // image_size):
        base = header["images"]["offset"] + i * image_size
        (name_index, assembly_index, type_start, type_count,
         exported_type_start, exported_type_count, entry_point_index,
         token, custom_attribute_start, custom_attribute_count) = struct.unpack_from(
            "<IiiIiIiIiI", blob, base
        )
        image_defs.append({
            "index": i,
            "name": cstring(strings, name_index),
            "assemblyIndex": assembly_index,
            "typeStart": type_start,
            "typeCount": type_count,
            "exportedTypeStart": exported_type_start,
            "exportedTypeCount": exported_type_count,
            "entryPointIndex": entry_point_index,
            "token": f"0x{token:08x}",
            "customAttributeStart": custom_attribute_start,
            "customAttributeCount": custom_attribute_count,
        })

    assembly_names = {}
    for i in range(header["assemblies"]["size"] // assembly_size):
        base = header["assemblies"]["offset"] + i * assembly_size
        image_index, token, referenced_start, referenced_count, name_index = struct.unpack_from(
            "<iIiiI", blob, base
        )
        assembly_names[i] = {
            "imageIndex": image_index,
            "name": cstring(strings, name_index),
            "token": f"0x{token:08x}",
            "referencedAssemblyStart": referenced_start,
            "referencedAssemblyCount": referenced_count,
        }

    # 已确认的 Il2CppDumper v31 记录尺寸和偏移。
    type_size = 88
    method_size = 36
    field_size = 12
    parameter_size = 12
    type_defs = []
    for i in range(header["typeDefinitions"]["size"] // type_size):
        base = header["typeDefinitions"]["offset"] + i * type_size
        name_index, namespace_index = struct.unpack_from("<II", blob, base)
        field_start, method_start = struct.unpack_from("<ii", blob, base + 32)
        method_count, _, field_count = struct.unpack_from("<HHH", blob, base + 64)
        type_defs.append({
            "index": i,
            "name": cstring(strings, name_index),
            "namespace": cstring(strings, namespace_index),
            "fieldStart": field_start,
            "fieldCount": field_count,
            "methodStart": method_start,
            "methodCount": method_count,
        })

    results = []
    for type_def in type_defs:
        target = TARGETS.get(type_def["name"])
        # TeamCherry 生成的部分类在元数据字符串表中带有空命名空间，
        # 即使其托管程序集实际属于 TeamCherry.TK2D。保留类名作为主要证据，
        # 并在下方展示记录到的命名空间。
        if target is None or (target["namespace"] != type_def["namespace"] and type_def["name"] != "tk2dSpriteCollectionData"):
            continue

        owner_image = next(
            (
                image for image in image_defs
                if image["typeStart"] <= type_def["index"] < image["typeStart"] + image["typeCount"]
            ),
            None,
        )
        owner_assembly = (
            assembly_names.get(owner_image["assemblyIndex"])
            if owner_image is not None else None
        )

        methods = []
        for method_index in range(type_def["methodStart"], type_def["methodStart"] + type_def["methodCount"]):
            base = header["methods"]["offset"] + method_index * method_size
            name_index, declaring, return_type, return_param, parameter_start, generic_container, token = struct.unpack_from("<IiiiiiI", blob, base)
            flags, iflags, slot, parameter_count = struct.unpack_from("<HHHH", blob, base + 28)
            name = cstring(strings, name_index)
            if not target["methods"] or name in target["methods"]:
                methods.append({
                    "metadataIndex": method_index,
                    "name": name,
                    "declaringType": declaring,
                    "returnTypeIndex": return_type,
                    "returnParameterToken": return_param,
                    "parameterStart": parameter_start,
                    "parameterCount": parameter_count,
                    "genericContainerIndex": generic_container,
                    "token": f"0x{token:08x}",
                    "flags": f"0x{flags:04x}",
                    "slot": slot,
                    "nativeAddress": None,
                })

        fields = []
        for field_index in range(type_def["fieldStart"], type_def["fieldStart"] + type_def["fieldCount"]):
            base = header["fields"]["offset"] + field_index * field_size
            name_index, type_index, token = struct.unpack_from("<I i I", blob, base)
            fields.append({
                "metadataIndex": field_index,
                "name": cstring(strings, name_index),
                "typeIndex": type_index,
                "token": f"0x{token:08x}",
            })

        results.append({
            **type_def,
            "ownerImage": owner_image,
            "ownerAssembly": owner_assembly,
            "methods": methods,
            "fields": fields,
        })

    return {
        "path": str(path),
        "sanity": "0xfab11baf",
        "version": version,
        "headerSize": cursor,
        "typeDefinitionCount": len(type_defs),
        "targetTypes": results,
        "nativeAddresses": "未知：需要匹配的 main NSO",
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--metadata", type=Path, default=Path("romfs/Data/Managed/Metadata/global-metadata.dat"))
    parser.add_argument("--output", type=Path, default=Path("output/reports/il2cpp-targets.json"))
    args = parser.parse_args()
    result = parse(args.metadata)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
