#include <Rcpp.h>
using namespace Rcpp;


#include "gdal_priv.h"
#include "gdalwarper.h"
#include "gdal_utils.h"  // for GDALWarpAppOptions
#include "gdalraster/gdalraster.h"


#include <Rinternals.h>


#define R_RGB(r,g,b)	  ((r)|((g)<<8)|((b)<<16)|0xFF000000)
#define R_RGBA(r,g,b,a)	((r)|((g)<<8)|((b)<<16)|((a)<<24))

SEXP C_native_rgb(SEXP b0, SEXP b1, SEXP b2, SEXP dm) {
    SEXP res_ = PROTECT(Rf_allocVector(INTSXP, Rf_length(b0)));
    for (int i = 0; i < Rf_length(b0); i++) {
        INTEGER(res_)[i] = (int)R_RGB(RAW(b0)[i], RAW(b1)[i], RAW(b2)[i]);
    }
    SEXP dim;
    dim = Rf_allocVector(INTSXP, 2);
    INTEGER(dim)[0] = INTEGER(dm)[1];
    INTEGER(dim)[1] = INTEGER(dm)[0];
    Rf_setAttrib(res_, R_DimSymbol, dim);
    Rf_setAttrib(res_, R_ClassSymbol, Rf_mkString("nativeRaster"));
    {
        SEXP chsym = Rf_install("channels");
        Rf_setAttrib(res_, chsym, Rf_ScalarInteger(3));
    }
    UNPROTECT(1);
    return res_;
}


SEXP C_native_rgba(SEXP b0, SEXP b1, SEXP b2, SEXP b3, SEXP dm) {
    SEXP res_ = PROTECT(Rf_allocVector(INTSXP, Rf_length(b0)));
    for (int i = 0; i < Rf_length(b0); i++) {
        INTEGER(res_)[i] = (int)R_RGBA(RAW(b0)[i], RAW(b1)[i], RAW(b2)[i], RAW(b3)[i]);
    }
    SEXP dim;
    dim = Rf_allocVector(INTSXP, 2);
    INTEGER(dim)[0] = INTEGER(dm)[1];
    INTEGER(dim)[1] = INTEGER(dm)[0];
    Rf_setAttrib(res_, R_DimSymbol, dim);
    Rf_setAttrib(res_, R_ClassSymbol, Rf_mkString("nativeRaster"));
    {
        SEXP chsym = Rf_install("channels");
        Rf_setAttrib(res_, chsym, Rf_ScalarInteger(4));
    }
    UNPROTECT(1);
    return res_;
}

// [[Rcpp::export]]
List gdal_warp_nara(CharacterVector dsn,
                              CharacterVector target_crs,
                              NumericVector target_extent,
                              IntegerVector target_dim,
                              NumericVector target_res,
                              IntegerVector bands,
                              CharacterVector resample,
                              LogicalVector silent,
                             CharacterVector band_output_type,
                              CharacterVector options,
                              CharacterVector dsn_outname,
                              LogicalVector include_meta) {


  //CharacterVector band_output_type = CharacterVector::create("Byte");

  GDALAllRegister();
  GDALDatasetH *poSrcDS;
  poSrcDS = static_cast<GDALDatasetH *>(CPLMalloc(sizeof(GDALDatasetH) * static_cast<size_t>(dsn.size())));

  for (int i = 0; i < dsn.size(); i++) {
    poSrcDS[i] = gdalraster::gdalH_open_dsn(dsn[i],   0);
    // unwind everything, and stop (why not unwind if all are null, message how many succeed)
    if (poSrcDS[i] == nullptr) {
      if (i > 0) {
        for (int j = 0; j < i; j++) GDALClose(poSrcDS[j]);
      }
      Rprintf("input source not readable: %s\n", (char *)dsn[i]);
      CPLFree(poSrcDS);
      Rcpp::stop("");
    }
  }

  // handle warp settings and options
  // we manually handle -r, -te, -t_srs, -ts, -of,
  // but the rest passed in as wo, to, oo, doo, or general (non general ones get -wo/-to/-oo/-doo prepended in R)
  char** papszArg = nullptr;

  bool write_dsn = false;
  if (EQUAL(dsn_outname[0], "")) {
    papszArg = CSLAddString(papszArg, "-of");
    papszArg = CSLAddString(papszArg, "MEM");

  } else {
    // no need for this because GTiff is the default (we don't get auto-choose)
    // bool do_format = true;
    // // first check if user requests a format
    // for (int iopt = 0; iopt < options.size(); iopt++) {
    //   if (EQUAL(options[iopt], "-of")) {
    //     do_format = false;
    //   }
    // }
    // if (do_format) {
    //  papszArg = CSLAddString(papszArg, "-of");
    //  papszArg = CSLAddString(papszArg, "GTiff");
    // }
    write_dsn = true;
  }

  if (!target_crs[0].empty()) {
    OGRSpatialReference *oTargetSRS = nullptr;
    oTargetSRS = new OGRSpatialReference;
    const char * strforuin = (const char *)target_crs[0];
    OGRErr target_chk =  oTargetSRS->SetFromUserInput(strforuin);
    if (target_chk != OGRERR_NONE) Rcpp::stop("cannot initialize target projection");
    const char *st = NULL;
    st = ((GDALDataset *)poSrcDS[0])->GetProjectionRef();

    papszArg = CSLAddString(papszArg, "-t_srs");
    papszArg = CSLAddString(papszArg, target_crs[0]);

    if(!st || !st[0]) {
        // we also should be checking if no geolocation arrays and no gcps
        Rcpp::warning("no source crs, target crs is ignored\n");
    }




  }

  if (target_extent.size() > 0) {
    double dfMinX = target_extent[0];
    double dfMaxX = target_extent[1];
    double dfMinY = target_extent[2];
    double dfMaxY = target_extent[3];

    papszArg = CSLAddString(papszArg, "-te");
    papszArg = CSLAddString(papszArg, CPLSPrintf("%.18g,", dfMinX));
    papszArg = CSLAddString(papszArg, CPLSPrintf("%.18g,", dfMinY));
    papszArg = CSLAddString(papszArg, CPLSPrintf("%.18g,", dfMaxX));
    papszArg = CSLAddString(papszArg, CPLSPrintf("%.18g,", dfMaxY));
  }

  if (target_dim.size() > 0) {
    int nXSize = target_dim[0];
    int nYSize = target_dim[1];
    papszArg = CSLAddString(papszArg, "-ts");
    papszArg = CSLAddString(papszArg, CPLSPrintf("%d", nXSize));
    papszArg = CSLAddString(papszArg, CPLSPrintf("%d", nYSize));
  }

  if (target_res.size() > 0) {
    double XRes = target_res[0];
    double YRes = target_res[1];
    if (! (XRes > 0 && YRes > 0)) {
      Rcpp::stop("invalid value/s for 'target_res' (not greater than zero)\n");
    }
    papszArg = CSLAddString(papszArg, "-tr");
    papszArg = CSLAddString(papszArg, CPLSPrintf("%f", XRes));
    papszArg = CSLAddString(papszArg, CPLSPrintf("%f", YRes));
  }
  if (resample.size() > 0) {
    papszArg = CSLAddString(papszArg, "-r");
    papszArg = CSLAddString(papszArg, resample[0]);
  }
  for (int gwopt = 0; gwopt < options.length(); gwopt++) {
    papszArg = CSLAddString(papszArg, options[gwopt]);
  }


  auto psOptions = GDALWarpAppOptionsNew(papszArg, nullptr);
  CSLDestroy(papszArg);
  GDALWarpAppOptionsSetProgress(psOptions, NULL, NULL );


    GDALDatasetH hRet = GDALWarp(dsn_outname[0], nullptr,
                                  static_cast<int>(dsn.size()), poSrcDS,
                                  psOptions, nullptr);

  GDALWarpAppOptionsFree(psOptions);

  CPLAssert( hRet != NULL );
  for (int si = 0; si < dsn.size(); si++) {
    GDALClose( (GDALDataset *)poSrcDS[si] );
  }
  CPLFree(poSrcDS);

  if (hRet == nullptr) {
    Rcpp::stop("data source could not be processed with GDALWarp api");
  }


  List outlist;
  if (write_dsn) {
    outlist.push_back(dsn_outname);

  } else {
    // Prepare to read bands
    int nBands;
     nBands = (int)GDALGetRasterCount(hRet);
     int nbands_to_read = (int)bands.size();
/// if user set bands to NULL, then all bands read (default is bands = 1L)
     if (bands[0] < 1) {
       nbands_to_read = nBands;
     }
    std::vector<int> bands_to_read(static_cast<size_t>(nbands_to_read));
    for (int i = 0; i < nbands_to_read; i++) {
      if (bands[0] >= 1) {
        bands_to_read[static_cast<size_t>(i)] = bands[i];
      } else {
        bands_to_read[static_cast<size_t>(i)] = i + 1;
      }
      if (bands_to_read[static_cast<size_t>(i)] > nBands) {
        GDALClose( hRet );
        stop("band number is not available: %i", bands[i]);
      }

    }
    if (bands_to_read.size() == 2) {
        Rcpp::stop("we cannot make a nativeRaster from 2 bands, must be 3, 4, or 1");
    }
    if (bands_to_read.size() > 4) {
        Rcpp::warning("we cannot make a nativeRaster from > 4 bands, using bands 1, 2, 3, 4 for rgba");
    }

    LogicalVector unscale = true;
    IntegerVector window(6);
    // default window with all zeroes results in entire read (for warp)
    // also supports vapour_raster_read  atm
    for (int i  = 0; i < window.size(); i++) window[i] = 0;


    outlist = gdalraster::gdal_read_band_values(((GDALDataset*) hRet),
                                                window,
                                                bands_to_read,
                                                band_output_type,
                                                resample,
                                                unscale);
  }

  List outlist_nara = List();

   R_xlen_t dimx =  ((GDALDataset*)hRet)->GetRasterXSize();
  R_xlen_t dimy =  ((GDALDataset*)hRet)->GetRasterYSize();

  // GREY
  if (outlist.size() == 1) {
    outlist_nara.push_back(C_native_rgb(outlist[0], outlist[0], outlist[0], IntegerVector::create(dimx, dimy)));
  }
  // RGB
  if (outlist.size() == 3) {
    outlist_nara.push_back(C_native_rgb(outlist[0], outlist[1], outlist[2], IntegerVector::create(dimx, dimy)));
  }
  // RGBA (we ignore bands above 4)
  if (outlist.size() >= 4) {
    outlist_nara.push_back(C_native_rgba(outlist[0], outlist[1], outlist[2], outlist[3], IntegerVector::create(dimx, dimy)));
  }


  if (include_meta[0]) {
  // shove the grid details on as attributes
  // get the extent ...
  double        adfGeoTransform[6];
  //poDataset->GetGeoTransform( adfGeoTransform );
  GDALGetGeoTransform(hRet, adfGeoTransform );
  double xmin = adfGeoTransform[0];
  double xmax = adfGeoTransform[0] + (double)dimx * adfGeoTransform[1];
  double ymin = adfGeoTransform[3] + (double)dimy * adfGeoTransform[5];
  double ymax = adfGeoTransform[3];


  const char *proj;
  proj = GDALGetProjectionRef(hRet);
  //https://gis.stackexchange.com/questions/164279/how-do-i-create-ogrspatialreference-from-raster-files-georeference-c
  outlist_nara.attr("dimension") = NumericVector::create(dimx, dimy);
  outlist_nara.attr("extent") = NumericVector::create(xmin, xmax, ymin, ymax);
  outlist_nara.attr("projection") = CharacterVector::create(proj);
  }
  GDALClose( hRet );
  return outlist_nara;
}
