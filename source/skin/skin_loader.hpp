#pragma once

namespace silkmodloader::skin {

/* Called after tk2dSpriteCollectionData::Init has returned. */
void OnCollectionInitialized(void* collection);

/* Called after the verified Material.set_mainTexture observer. */
void OnStandaloneTextureAssigned(void* texture);

/* Called after the verified SpriteRenderer/Image sprite observers. */
void OnSpriteAssigned(void* sprite);

enum class TextureVisitResult { Unchanged, Applied, Retry };

/* Main-thread discovery uses the same named replacement path as the hooks. */
TextureVisitResult OnLoadedTextureDiscovered(void* texture);
void OnMainThreadFrame();

} // namespace silkmodloader::skin
