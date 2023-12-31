
vapour::gdal_raster_dsn(sds:::ozgrab_bag_sources[5], target_ext = c(-2499000,  2615000, -4963000,  -872000),
                        target_crs = "EPSG:9473", target_dim = c(1200, 0),
                        out_dsn = "file.tif", resample = "cubic", options = c("-co", "COMPRESS=WEBP"))


