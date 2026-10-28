// Native ET detail material. Install via DynamicShader before LoadWorldMap.
// Outside scripts/: no automatic override, no runtime remap/lightmap loss.

textures/twy_brickstiles/brick_dirt_7
{
	qer_editorimage textures/twy_brickstiles/brick_dirt_7.jpg
	{
		map $lightmap
		rgbGen identity
	}
	{
		map textures/twy_brickstiles/brick_dirt_7.jpg
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
	{
		map textures/tce2/details/brick.tga
		blendFunc GL_DST_COLOR GL_SRC_COLOR
		rgbGen identity
		tcMod scale 8 8
		detail
	}
}
