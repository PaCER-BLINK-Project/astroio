#include "images.hpp"
#include "files.hpp"
#include <iomanip>
#include <libnova/sidereal_time.h> // ln_get_apparent_sidereal_time
#include <libnova/julian_day.h>    // ln_get_julian_from_timet

namespace {
   double cut_to_range(double& sid_local_h)
   {
      if( sid_local_h > 24.00 ){
         sid_local_h = sid_local_h-24.00;
      }
   
      if( sid_local_h < 0 ){
         sid_local_h = sid_local_h+24.00;
      }
   
      return sid_local_h;
   }
 
   double get_local_sidereal_time(double uxtime_d,double geo_long_deg,double& jd_out)
   {
      time_t uxtime = (time_t)uxtime_d;
      jd_out = ln_get_julian_from_timet( &uxtime );
      // printf("DEBUG : jd(%d = floor(%.8f)) = %.8f\n",(int)uxtime,uxtime_d,jd_out);
      jd_out += (uxtime_d - uxtime)/(24.00*3600.00);
      // printf("DEBUG : jd_prim = %.8f\n",jd_out);

      // which to use ???
      double sid_greenwich_h = ln_get_apparent_sidereal_time(jd_out);
      // printf("DEBUG : sid_greenwich = %.8f [h] = %.8f [deg]\n",sid_greenwich_h,sid_greenwich_h*15.00);
  
      double sid_local_h = sid_greenwich_h + geo_long_deg/15.00;
      // printf("DEBUG : sid_local = %.8f + %.8f = %.8f [h] = %.8f [deg]\n",sid_greenwich_h,geo_long_deg/15.00,sid_local_h,sid_local_h*15.00);

      cut_to_range(sid_local_h);

      return sid_local_h;  
   }

   void fixCoordHdr( double ra_center_deg, double dec_center_deg, double lst_hours, double long_deg, double lat_deg, double& xi, double& eta )
   {
      const double deg2rad = (M_PI/180.00);

      double lat_radian = lat_deg*deg2rad;
      double long_radian = long_deg*deg2rad;

      double dec_center_rad=dec_center_deg*deg2rad;
      double ra_center_rad=ra_center_deg*deg2rad;
      double ra_center_h=ra_center_deg/15.0;

      double ha_hours=lst_hours-ra_center_h;
      double ha_radians=(ha_hours*15.00)*deg2rad;

      // printf("values = %.8f %.8f %.8f , %.8f\n",lat_radian,ra_center_rad,dec_center_rad,ha_radians);
      double cosZ = sin(lat_radian)*sin(dec_center_rad) + cos(lat_radian)*cos(dec_center_rad)*cos(ha_radians);
      double tanZ = sqrt(1.00-cosZ*cosZ)/cosZ;

      // printf("tanZ = %.8f\n",tanZ);

      // Parallactic angle
      // http://www.gb.nrao.edu/~rcreager/GBTMetrology/140ft/l0058/gbtmemo52/memo52.html
      double tan_chi = sin(ha_radians)/( cos(dec_center_rad)*tan(lat_radian) - sin(dec_center_rad)*sin(ha_radians)  );

      // $lat_radian $dec_radian $ha_radian
      // printf("DEBUG : values2 : %.8f %.8f %.8f\n",lat_radian,dec_center_rad,ha_radians);
      double chi_radian = atan2( sin(ha_radians) , cos(dec_center_rad)*tan(lat_radian) - sin(dec_center_rad)*cos(ha_radians) );

      // printf("chi_radian = %.8f\n",chi_radian);

      // there is a - sign in the paper, but Randall says it's possibly wrong:
      // so I stay with NO - SIGN version
      xi=tanZ*sin(chi_radian);
      eta=tanZ*cos(chi_radian);

      // printf("xi = %.8f\n",xi);
      // printf("eta = %.8f\n",eta);
   }    
}

void Images::save_fits_file(const std::string filename, float* data, long side_x, long side_y){
    FITS fitsImage;
    FITS::HDU hdu;
    hdu.set_image(data,  side_x, side_y);
    
    // calculate LST :
    double jd,xi,eta;
    double lst_hours = get_local_sidereal_time( obsInfo.startTime, obsInfo.geo_long_deg, jd );
    fixCoordHdr( ra_deg, dec_deg, lst_hours, obsInfo.geo_long_deg, obsInfo.geo_lat_deg, xi, eta );
    
    
    // hdu.add_keyword("TIME", static_cast<long>(obsInfo.startTime), "Unix time (seconds)");
    // hdu.add_keyword("MILLITIM", msElapsed, "Milliseconds since TIME");
    // hdu.add_keyword("INTTIME", integrationTime, "Integration time (s)");
    // hdu.add_keyword("COARSE_CHAN", obsInfo.coarseChannel, "Receiver Coarse Channel Number (only used in offline mode)");
    hdu.add_keyword("CTYPE1", std::string { "RA---SIN"}, "");
    hdu.add_keyword("CRPIX1", side_x / 2 + 1, "" );   
    hdu.add_keyword("CDELT1", pixscale_ra, "Pixscale" );
    hdu.add_keyword("CRVAL1", ra_deg , "RA value in deg." ); // RA of the centre 
    hdu.add_keyword("CUNIT1", std::string { "deg"} , "" );

    hdu.add_keyword("CTYPE2", std::string {"DEC--SIN"}, "") ;
    hdu.add_keyword("CRPIX2", side_y / 2 + 1 , "" );   
    hdu.add_keyword("CDELT2", pixscale_dec , "Pixscale" );
    hdu.add_keyword("CRVAL2", dec_deg , "DEC value in deg." ); // RA of the centre 
    hdu.add_keyword("CUNIT2", std::string { "deg"} , "" );
    
    // additional keywords required for getting correct coordintes :
    hdu.add_keyword("PV2_1", xi  , "" );
    hdu.add_keyword("PV2_2", eta , "" );
    
    fitsImage.add_HDU(hdu);
    fitsImage.to_file(filename);
}

void Images::to_fits_files(const std::string& directory_path, bool save_as_complex, bool save_imaginary) {
    if(on_gpu()) to_cpu();
    MemoryBuffer<float> img_real(this->image_size(), false, false);
    MemoryBuffer<float> img_imag(this->image_size(), false, false);
    blink::imager::create_directory(directory_path);
    for(size_t interval {0}; interval < this->integration_intervals(); interval++){
        for(size_t fine_channel {0}; fine_channel < this->nFrequencies; fine_channel++){
            std::complex<float> *current_data {this->data() + this->image_size() * this->nFrequencies * interval + fine_channel * this->image_size()}; 
            std::stringstream full_directory;
            full_directory << std::setfill('0') << directory_path << "/" << "start_time_" << obsInfo.startTime << \
                "_" << "int_" << std::setw(2) << interval <<  std::setw(0) << "_coarse_" <<  std::setw(3) <<  obsInfo.coarseChannel <<  std::setw(0) << "_fine_ch" <<  std::setw(2) << fine_channel;
            std::string full_directory_str {full_directory.str()};
            if(save_as_complex){
                std::string filename {full_directory_str + "_image.fits"};
                std::complex<float>* p_data = this->at(interval, fine_channel);
                save_fits_file(filename, reinterpret_cast<float*>(p_data), this->side_size, this->side_size *2 );
            }else{
                for(size_t i {0}; i < this->image_size(); i++){
                    img_real[i] = current_data[i].real();
                }
                save_fits_file(full_directory_str + "_image_real.fits", img_real.data(), this->side_size, this->side_size );
                if(save_imaginary){
                    for(size_t i {0}; i < this->image_size(); i++){
                        img_imag[i] = current_data[i].imag();
                    }
                    save_fits_file(full_directory_str + "_image_imag.fits", img_imag.data(), this->side_size, this->side_size );
                }
            }
        }
    }
}
