#' Read a gdal raster to nativeRaster
#'
#' Note can use `scale=0,1` to convert literally any raster to image this way too.
#'
#' @param dsn a raster source for GDAL
#' @param target_crs target crs (optional)
#' @param target_ext target extent (optional, xmin,xmax,ymin,ymax)
#' @param target_dim target dimension (optional, ncol,nrow  - one value can be 0)
#' @param target_res target resolution (optional, resx, resy)
#' @param bands  which bands to read (we could read just the second green band, for example)
#' @param resample optional resampling algo (near is default, use bilinear or cubic or see GDAL doc)
#' @param silent keep quiet
#' @param options  options for the gdalwarp binary (can use c("-tr", xres, yres) in place of target_res for example)
#'
#' @return list in ximage() gdal_raster_ format as per vapour package
#' @export
#' @importFrom vapour vapour_raster_info
#' @examples
#' dsn <- "vrt:///vsicurl/http://s3.amazonaws.com/com.modestmaps.bluemarble/0-r0-c0.jpg"
#' dsn <- paste0(dsn, "?a_ullr=-20037508,20037508,20037508,-20037508&a_srs=EPSG:900913")
#' gdal_raster_nara(dsn, target_dim = c(36, 36))
#' #png <- system.file("img", "Rlogo.png", package="png")
#' ## for dumb imagery we need to specify NO_GEOTRANSFORM or set the native affine transform
#' #gdal_raster_nara(sprintf("vrt://%s?a_gt =0,1,0,76,0,-1", png)) #, options = c("-to", "SRC_METHOD=NO_GEOTRANSFORM"))
gdal_raster_nara <- function(dsn, target_crs = "", target_ext = numeric(), target_dim = numeric(), target_res = numeric(),
    bands = NULL, resample = "near", silent = FALSE,  options = "") {

    info <- vapour_raster_info(dsn[1])
    #if (info$geotransform)
     if (is.null(bands)) {
        nbands <- info$bands
        bands <- seq(min(c(nbands, 4L)))
     }

    gdal_warp_nara(dsn = dsn, target_crs = target_crs, target_extent = target_ext, target_dim = target_dim, target_res = target_res,
    bands  = bands, resample = resample, silent = silent, options = options,
        band_output_type = "Byte",
        dsn_outname = "",
    include_meta = TRUE)
}

