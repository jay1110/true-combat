// The stock *_dark shader aliases force alpha .25. These three footer-only
// aliases keep the original ET textures and their alpha masks, while ignoring
// menu palette tint and opacity. No raster duplication or modification.
tce2_ui/publisher_atvi
{
    nopicmip
    nomipmaps
    nocompress
    {
        clampmap ui/assets/logo_atvi.tga
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen identity
        alphaGen identity
    }
}

tce2_ui/publisher_id
{
    nopicmip
    nomipmaps
    nocompress
    {
        clampmap ui/assets/logo_id.tga
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen identity
        alphaGen identity
    }
}

tce2_ui/publisher_sd
{
    nopicmip
    nomipmaps
    nocompress
    {
        clampmap ui/assets/logo_sd.tga
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen identity
        alphaGen identity
    }
}

// Small offset silhouettes use the same original alpha masks. They improve
// contrast on bright scenery without a footer bar, plate or raster copy.
tce2_ui/publisher_atvi_shadow
{
    nopicmip
    nomipmaps
    nocompress
    {
        clampmap ui/assets/logo_atvi.tga
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen const ( 0 0 0 )
        alphaGen const 0.8
    }
}

tce2_ui/publisher_id_shadow
{
    nopicmip
    nomipmaps
    nocompress
    {
        clampmap ui/assets/logo_id.tga
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen const ( 0 0 0 )
        alphaGen const 0.8
    }
}

tce2_ui/publisher_sd_shadow
{
    nopicmip
    nomipmaps
    nocompress
    {
        clampmap ui/assets/logo_sd.tga
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
        rgbGen const ( 0 0 0 )
        alphaGen const 0.8
    }
}
