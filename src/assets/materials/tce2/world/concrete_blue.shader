// Native ET detail material. Install via DynamicShader before LoadWorldMap.
// Outside scripts/: no automatic override, no runtime remap/lightmap loss.

textures/bruce_trim/concrete_blue
{
	qer_editorimage textures/bruce_crete/concrete_blue.tga
	{
		map $lightmap
		rgbGen identity
	}
	{
		map textures/bruce_crete/concrete_blue.tga
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
	{
		map textures/tce2/details/concrete.tga
		blendFunc GL_DST_COLOR GL_SRC_COLOR
		rgbGen identity
		tcMod scale 8 2
		detail
	}
}
