// --------------------------------------------------------------------------------------------
// based on mwaconfig.h(cpp) and metafitsfile.h(cpp) in cotter
// --------------------------------------------------------------------------------------------
#include "metafits_mapping.hpp"
#include "astroio.hpp"
#include <math.h>
#include <fitsio.h>
#include <iostream>

#define MWA_LATTITUDE -26.703319        // Array latitude. degrees North
#define MWA_LONGITUDE 116.67081         // Array longitude. degrees East
#define MWA_HEIGHT 377               // Array altitude. meters above sea level
#include <vector>
#include <cstring>
#include <string>
#include <map>

// #include "antenna_positions.h"

// struct fitsfile;

// based on mwaconfig.h(cpp) and metafitsfile.h(cpp) in cotter 

#define DEFUALT_STRING_VALUE "Unknown"

/*
#define MWA_LATTITUDE -26.703319        // Array latitude. degrees North
#define MWA_LONGITUDE 116.67081         // Array longitude. degrees East
#define MWA_HEIGHT 377               // Array altitude. meters above sea level
*/

namespace { // Anonymous namespace
using namespace std;

class InputMapping
{
public :
   int input;
   int antenna;
   string szAntName;
   char pol;
   int delta;
   int flag;  
   
   double x;
   double y;
   double z;

   InputMapping() 
   : input(-1), antenna(-1), pol('U'), delta(0), flag(0), x(0), y(0), z(0)
   {};

//   static int read_mapping_file( std::vector<InputMapping>& inputs , const char* filename="instr_config.txt" );
};

class CObsMetadata 
{
public :
   CObsMetadata( const char* filename=DEFUALT_STRING_VALUE );
   
   std::string m_filename; // Meta data file (txt,metafits etc).
   
   // 
   fitsfile *_fptr;
   bool m_bHasMetaFits;
   
   // Antenna positions :   
   //CAntennaPositions m_AntennaPositions;
   std::vector<InputMapping> antenna_positions;
   size_t nInputs;                 // the number of inputs to the correlator
   size_t nScans;                  // the number of time samples
   size_t nChannels;               // the number of spectral channels
   enum CorrType { None, CrossCorrelation, AutoCorrelation, BothCorrelations } correlationType;
   double integrationTime;         // per time sample, in seconds
   double centralFrequencyMHz;     // observing central frequency and 
   double bandwidthMHz;            // bandwidth (MHz)
   double haHrs, raHrs, decDegs;          // ra,dec of phase centre.
   double haHrsStart;              // the HA of the phase center at the start of the integration
   double refEl, refAz;            // the el/az of the normal to the plane of the array (radian)
   int    year, month, day;        // date/time in UTC.
   int    refHour, refMinute;
   double refSecond;
   //bool   invertFrequency;         // flag to indicate that freq decreases with increasing channel number.
   bool   conjugate;               // conjugate the final vis to correct for any sign errors
   bool   geomCorrection;          // apply geometric phase correction
   std::string fieldName;
   std::string polProducts;

   std::vector<int> input_mapping;

   // time :
   double startUnixTime;
   double dateFirstScanMJD;        // The central time of the first time step. Not in header file, derived from it.
   
   // array location:
   double geo_long;
   double geo_lat;
   double geo_height;
   
   // part from MWAHeaderExt
   int gpsTime;
   std::string observerName, projectName, gridName, mode, mwaPyVersion, mwaPyDate, metaDataVersion;
   std::string filename; // name of file in the METADATA
   int delays[16], subbandGains[24], subbandNumbers[24];
   bool hasCalibrator, hasGlobalSubbandGains;
   int centreSBNumber;
   //double fiberFactor;
   double tilePointingRARad, tilePointingDecRad;
   double dateRequestedMJD;
   
   bool HasMetaFits(){
      return m_bHasMetaFits;
   }

   double GetDateLastScanMJD() const
   {
      return dateFirstScanMJD + (integrationTime/86400.0)*nScans;
   }
   
   // TODO :
//   double GetStartDateMJD() const;
   double GetDateFirstScanFromFields() const;
   
   //------------------------------------------------------------------------------------------------------------------------------
   // Read metadata 
   // reading meta data from text file : 
   //bool ReadMetaDataTxt( const char* filename );
   
   bool ReadMetaFitsFile( const char* filename );
   
   bool ReadMetaData( const char* filename );
   
   // reading of antenna positions :
   int ReadAntPositions();

   // parsing Metafits file information :
   bool parseKeyword( const std::string& keyName, const std::string& keyValue );
   bool parseFitsString(const char* valueStr, std::string& outString );
   std::string stripBand(const std::string& input);
   bool parseFitsDate(const char* valueStr, int& year, int& month, int& day, int& hour, int& min, double& sec);
   double parseFitsDateToMJD(const char* valueStr);
   bool parseIntArray(const char* valueStr, int* values, size_t count);
   bool parseBool(const char* valueStr);

   // error checking :
   bool checkStatus(int status, const char* szErrorMsg );
   

   // void Validate(bool lockPointing);
   private:
      CObsMetadata(const CObsMetadata &) { }
      void operator=(const CObsMetadata &) { }
      
      static bool isDigit(char c) { return c >= '0' && c <= '9'; }
};

// #include <cstdlib>
// #include <cmath>

// #include <fstream>
// #include <sstream>
// #include <iostream>

// #include <stdexcept>

// #include "geometry.h"
// #include "metafitsfile.h"

#define VEL_FACTOR  1.204         // the velocity factor of electic fields in RG-6 like coax.

#define DEFAULT_VALUE -1000

bool CObsMetadata::checkStatus(int status, const char* szErrorMsg )
{
   if( status ){
      printf("%s, status = %d\n",szErrorMsg,status);      
      return false;      
   }
   
   return true;
}

CObsMetadata::CObsMetadata( const char* filename ) :
        _fptr(NULL), m_bHasMetaFits(false),
        nInputs(0),
        nScans(0),
        nChannels(0),
        correlationType(None),
        integrationTime(0.0),
        centralFrequencyMHz(0.0),
        bandwidthMHz(0.0),
        haHrs(DEFAULT_VALUE),
        raHrs(DEFAULT_VALUE),
        decDegs(DEFAULT_VALUE),
        haHrsStart(DEFAULT_VALUE),
        refEl(M_PI*0.5),
        refAz(0.0),
        year(0),
        month(0),
        day(0),
        refHour(0),
        refMinute(0),
        refSecond(0.0),
        conjugate(false),
        geomCorrection(true),
        fieldName(),
        polProducts("XXXYYXYY"),
        geo_long(MWA_LONGITUDE), geo_lat(MWA_LATTITUDE), geo_height(MWA_HEIGHT),
        startUnixTime(0.00),
        
        // from MWAHeaderExt
        gpsTime(0), observerName("Unknown"), projectName("Unknown"),
        gridName("Unknown"), mode("Unknown"),
        hasCalibrator(false), hasGlobalSubbandGains(false),
        centreSBNumber(0),
        //fiberFactor(VEL_FACTOR),
        tilePointingRARad(0.0), tilePointingDecRad(0.0),
        dateRequestedMJD(0.0)

{
   for(size_t i=0; i!=16; ++i) delays[i] = 0;
   for(size_t i=0; i!=24; ++i) subbandGains[i] = 0;
   
   if( filename && strlen(filename) ){
      m_filename = filename;
   }
}


bool CObsMetadata::ReadMetaData( const char* filename )
{
   if( strlen(m_filename.c_str())<=0 || strcmp(m_filename.c_str(),filename) ){
      m_filename = filename;
   }

   if( strlen(m_filename.c_str()) ){
      //if( strstr(m_filename.c_str(),".txt") ){
      //   return ReadMetaDataTxt( m_filename.c_str() );
      //}
      if( strstr(m_filename.c_str(),".metafits") ){
         return ReadMetaFitsFile( m_filename.c_str() );
      }
   }
   
   printf("ERROR : no filename specified\n");
   return false;   
}


//bool CObsMetadata::ReadMetaDataTxt( const char* filename )
//{
//   if( filename && strlen(filename) ){
//      if( !std::filesystem::exists( filename ) ){
//         printf("ERROR: filename %s does not exist\n",filename);
//         return false;
//      }
//   }else{
//      printf("ERROR : empty filename provided -> cannot continue\n");
//      return false;
//   }
//
////   clear();
//   int n_ant_index = 0;
//   MyFile file(filename);
//   const char* pLine;
//   if(!file.IsOpened()){
//      file.Open( filename );
//   }
//   while(pLine = file.GetLine(TRUE)){
//      if(strlen(pLine) && pLine[0]!='#'){
//         MyParser pars=pLine;
//         CMyStrTable items;
//         pars.GetItems( items );
//         
//         if( strcmp(items[0].c_str(), "HA" ) == 0 ){
//            haHrs = atof( items[2].c_str() )/15.00; // deg -> h 
//         }
//         if( strcmp(items[0].c_str(), "RA" ) == 0 ){
//            double raDegs = atof( items[2].c_str() );
//            raHrs = raDegs / 15.00; // deg -> h 
//            tilePointingRARad = raDegs*(M_PI/180.00);
//         }
//         if( strcmp(items[0].c_str(), "DEC" ) == 0 ){
//            decDegs = atof( items[2].c_str() ); // deg -> h 
//            tilePointingDecRad = decDegs*(M_PI/180.00);
//         }
//         if( strcmp(items[0].c_str(), "UXTIME" ) == 0 || strcmp(items[0].c_str(), "UNIXTIME" ) == 0 || strcmp(items[0].c_str(), "UNIX_TIME" ) == 0 || strcmp(items[0].c_str(), "START_UNIXTIME" ) == 0 ){
//            startUnixTime = atof( items[2].c_str() ); // deg -> h 
//         }
//         
//         // TODO : add more keywords as needed
//      }
//   }
//   
//   printf("DEBUG : read metadata from text file %s : HA = %.8f [h] = %.8f [deg]\n",filename,haHrs,haHrs*15.00);
//   
//   if(strcmp(m_filename.c_str(),filename)){
//      m_filename = filename;
//   }
//   m_bHasMetaFits = true;
//   return true;
//}

bool CObsMetadata::ReadMetaFitsFile( const char* filename )
{
//   fitsfile *_fptr = NULL; 
   int status = 0;
   
   if(fits_open_file(&_fptr, filename, READONLY, &status)){
      printf("ERROR : could not read FITS file %s\n",filename);
      _fptr = NULL;
      return false;
   }

   int hduCount;
   fits_get_num_hdus(_fptr, &hduCount, &status);
//   checkStatus(status);

   if(hduCount < 2){
      printf("ERROR : FITS file %s has less than 2 keywords -> cannot continue\n",filename);
      return false;     
   }
   
   // TODO : add reading FITS file here
   int hduType;
   fits_movabs_hdu(_fptr, 1, &hduType, &status);
   if( !checkStatus(status, "ERROR calling fits_movabs_hdu") ){ return false; }
        
   int keywordCount;
   fits_get_hdrspace(_fptr, &keywordCount, 0, &status);
   if( !checkStatus(status, "ERROR calling fits_get_hdrspace") ){ return false; }

   // initailise some keywords 
   tilePointingDecRad = DEFAULT_VALUE;
   tilePointingRARad  = DEFAULT_VALUE;
   raHrs   = DEFAULT_VALUE;
   decDegs = DEFAULT_VALUE;
   
   for(int i=0; i!=keywordCount; ++i){
      char keyName[80], keyValue[80], *keyValueLong, szErrorMsg[1024];
      fits_read_keyn(_fptr, i+1, keyName, keyValue, 0, &status);
      if( status ){
         printf("ERROR : could not read information about keyword %d using fits_read_keyn in fitsfile %s, status = %d\n",i,filename,status);
         return false;
      }

      std::string keyValueStr = keyValue;
      if(keyValueStr.size()>=3  && keyValueStr[0]=='\'' && keyValueStr[keyValueStr.size()-1]=='\'' && keyValueStr[keyValueStr.size()-2]=='&')
      {
         fits_read_key_longstr(_fptr, keyName, &keyValueLong, NULL, &status);
         if(status == 0){
            keyValueStr = std::string("'") + keyValueLong;
         }
         if(keyValueStr[keyValueStr.size()-1] == '&'){
            keyValueStr[keyValueStr.size()-1] = '\'';
         }else{
            keyValueStr += '\'';
         }
         if( status ){
            printf("ERROR : could not read keyword %d/%s using fits_read_key_longstr from fitsfile %s, status = %d\n",i,keyName,filename,status);
            return false;
         }
       
         fffree(keyValueLong, &status); // use the short name; some fftw version don't know the long name
         if( !checkStatus(status,"ERROR : fffree failed on variable keyValueLong") ){
            return false;
         }
      }

      parseKeyword( keyName, keyValueStr );
   }

   correlationType = CObsMetadata::BothCorrelations;

   if( tilePointingDecRad == DEFAULT_VALUE || tilePointingRARad == DEFAULT_VALUE ){
      printf("The metafits file does not specify a pointing direction (keywords RA and DEC)");
      return false;
   }else{
      if(raHrs == DEFAULT_VALUE || decDegs == DEFAULT_VALUE )
        {
                printf("The metafits file does not specify a phase centre; will use pointing centre as phase\n");
                printf("centre, unless overridden on command line.\n");
                
                raHrs = tilePointingRARad * (12.0/M_PI);
                decDegs = tilePointingDecRad * (180.0/M_PI);
        }
   }
   
   // calculate other variables :
   dateFirstScanMJD = GetDateFirstScanFromFields();
   
   // for now only read if not read earlier :
   if( antenna_positions.size() == 0 ){
      ReadAntPositions();
   }else{
   }
   
   if( _fptr ){
      if(fits_close_file(_fptr, &status)){
         printf("ERROR : could not close FITS file %s (fits_close_file status = %d)\n",filename,status);
         return false;
      }
      
      _fptr = NULL;
   }
   
   if(strcmp(m_filename.c_str(),filename)){
      m_filename = filename;
   }
   m_bHasMetaFits = true;
   
/*   printf("DEBUG metadata reading (WARNING - some hardcoded values here - TODO !!!) ha = %.8f [h] , uxtime = %.8f:\n",haHrs,startUnixTime);
   if( haHrs < -900 && false ){ 
      printf("WARNING : using hardcoded value of HA and UTC  !!!\n");
      haHrs =  -20.89187500;
      // ux2sid 1419609944
      // Sidereal time at GeoLong=116.67081500 [deg] (MWA) is  6.21215971 [h] = 93.18239562 [deg]
      startUnixTime = 1419609944.00;
   }*/
   printf("DEBUG metadata reading:\n");   
   printf("\thaHrs = %.8f [h]\n",haHrs);
   printf("\traHrs = %.8f [h] , tilePointingRARad = %.8f [rad]\n",raHrs,tilePointingRARad);
   printf("\tdecDegs = %.8f [deg] , tilePointingDecRad = %.8f [rad]\n",decDegs,tilePointingDecRad);
   printf("\tstartUnixTime = %.8f [sec]\n",startUnixTime);
   
   return true;
}

bool CObsMetadata::parseKeyword( const std::string& keyName, const std::string& keyValue )
{
        if(keyName == "SIMPLE" || keyName == "BITPIX" || keyName == "NAXIS" || keyName == "EXTEND" || keyName == "CONTINUE")
                ; //standard FITS headers; ignore.
        else if(keyName == "GPSTIME")
                gpsTime = atoi(keyValue.c_str());
        else if(keyName == "FILENAME")
        {
                if( !parseFitsString(keyValue.c_str(), filename ) ){
                   return false;
                }
                // filename = parseFitsString(keyValue.c_str());
                fieldName = stripBand(filename);
        }
        else if(keyName == "DATE-OBS")
        {
                parseFitsDate(keyValue.c_str(), year, month, day, refHour, refMinute, refSecond);
                dateRequestedMJD = parseFitsDateToMJD(keyValue.c_str());
                
                // unixtime : 
                // time_t get_gmtime_from_string( const char* szGmTime , const char* format="%Y%m%d_%H%M%S" );
                char szTmp[128];
                sprintf(szTmp,"%04d%02d%02d_%02d%02d%02d",year, month, day, refHour, refMinute, int(refSecond));
                startUnixTime = 0; // CRISTIAN get_gmtime_from_string( szTmp, "%Y%m%d_%H%M%S" );
                
                startUnixTime += (refSecond-int(refSecond));
        }
        else if(keyName == "RAPHASE")
                raHrs = atof(keyValue.c_str()) * (24.0 / 360.0);
        else if(keyName == "DECPHASE")
                decDegs = atof(keyValue.c_str());
        else if(keyName == "HA"){ 
              int h,m;
              double s;
              const char* szHA = keyValue.c_str();
              
              int ret = sscanf(szHA,"%d:%02d:%lf",&h,&m,&s);
              if( ret != 3 ){
                 ret = sscanf(szHA,"'%d:%02d:%lf'",&h,&m,&s);
                 if( ret != 3 ){
                    printf("ERROR : when parsing HA keyword = |%s| in metafits file\n",szHA);
                 }
              }
   
              double ha;
              ha = (m*60.00 + s)/3600.00 + fabs(h);
              if( h < 0 ){
                 ha = -ha;
              }
              haHrs = ha;
        }
        else if(keyName == "RA")
                tilePointingRARad = atof(keyValue.c_str()) * (M_PI / 180.0);
        else if(keyName == "DEC")
                tilePointingDecRad = atof(keyValue.c_str()) * (M_PI / 180.0);
        else if(keyName == "GRIDNAME"){
                // gridName = parseFitsString(keyValue.c_str());
                if( !parseFitsString(keyValue.c_str(), gridName ) ){
                   return false;   
                }
        }else if(keyName == "CREATOR"){
                if( !parseFitsString(keyValue.c_str(),observerName) ){
                   return false;
                }
        }else if(keyName == "PROJECT")
                if( !parseFitsString(keyValue.c_str(),projectName) ){ 
                   return false;
                }
        else if(keyName == "MODE")
                if( !parseFitsString(keyValue.c_str(),mode) ){
                   return false;
                }
        else if(keyName == "DELAYS"){
                if( !parseIntArray(keyValue.c_str(), delays, 16) ){
                   return false;
                }
        }else if(keyName == "CALIBRAT")
                hasCalibrator = parseBool(keyValue.c_str());
        else if(keyName == "CENTCHAN")
                centreSBNumber = atoi(keyValue.c_str());
        else if(keyName == "CHANGAIN") {
                if( !parseIntArray(keyValue.c_str(), subbandGains, 24) ){
                   return false;
                }
                hasGlobalSubbandGains = true;
        }
        //else if(keyName == "FIBRFACT")
        //      fiberFactor = atof(keyValue.c_str());
        else if(keyName == "INTTIME")
                integrationTime = atof(keyValue.c_str());
        else if(keyName == "NSCANS")
                nScans = atoi(keyValue.c_str());
        else if(keyName == "NINPUTS")
                nInputs = atoi(keyValue.c_str());
        else if(keyName == "NCHANS")
                nChannels = atoi(keyValue.c_str());
        else if(keyName == "BANDWDTH")
                bandwidthMHz = atof(keyValue.c_str());
        else if(keyName == "FREQCENT")
                centralFrequencyMHz = atof(keyValue.c_str());
        else if(keyName == "CHANNELS"){
                if( !parseIntArray(keyValue.c_str(), subbandNumbers, 24) ){
                   return false;
                }
        }else if(keyName == "DATESTRT")
                ; //parseFitsDate(keyValue, year, month, day, refHour, refMinute, refSecond);
        else if(keyName == "DATE")
                ; // Date that metafits was created; ignored.
        else if(keyName == "VERSION")
                metaDataVersion = keyValue.c_str();
        else if(keyName == "MWAVER"){
                if( !parseFitsString(keyValue.c_str(),mwaPyVersion) ){
                   return false;
                }
        }else if(keyName == "MWADATE"){
                if( !parseFitsString(keyValue.c_str(),mwaPyDate) ){
                   return false;
                }
        }else if(keyName == "TELESCOP")
                ; // Ignore; will always be set to 'MWA'
        else if( keyName == "EXPOSURE" || keyName == "MJD" || keyName == "LST" || keyName == "HA" || keyName == "AZIMUTH" || keyName == "ALTITUDE" || keyName == "SUN-DIST" || keyName == "MOONDIST" || 
                 keyName == "JUP-DIST" || keyName == "GRIDNUM" || keyName == "RECVRS" || keyName == "CHANNELS" || keyName == "SUN-ALT" || keyName == "TILEFLAG" || keyName == "NAV_FREQ" || 
                 keyName == "FINECHAN" || keyName == "TIMEOFF" )
                ; // Ignore these fields, they can be derived from others.
        else{
                // printf("Ignored keyword: %s\n",keyName.c_str());
                return false;
        }

        return true;
}

bool CObsMetadata::parseFitsString(const char* valueStr, std::string& outString )
{
        if(valueStr[0] != '\''){
           // printf("ERROR: could not parse string %s\n",valueStr);
           return false;
        }
        
        std::string value(valueStr+1);
        if((*value.rbegin())!='\''){
           // printf("ERROR : could not parse string : %s\n",valueStr);
           return false;
        }
        int s = value.size() - 1;
        while(s > 0 && value[s-1]==' ') --s;
        
        outString = value.substr(0, s);
        
        return true;
}

std::string CObsMetadata::stripBand(const std::string& input)
{
        if(!input.empty())
        {
                size_t pos = input.size()-1;
                while(pos>0 && isDigit(input[pos]))
                {
                        --pos;
                }
                if(pos > 0 && input[pos] == '_')
                {
                        int band = atoi(&input[pos+1]);
                        if(band > 0 && band <= 256)
                        {
                                return input.substr(0, pos);
                        }
                }
        }
        return input;
}

bool CObsMetadata::parseFitsDate(const char* valueStr, int& year, int& month, int& day, int& hour, int& min, double& sec)
{
        std::string dateStr;
        if( !parseFitsString(valueStr,dateStr) ){
           return false;
        }
        if(dateStr.size() != 19){
           //      throw std::runtime_error("Error parsing fits date");
           printf("ERROR : when parsing FITS date %s\n",valueStr);
           return false;
        }
        year = (dateStr[0]-'0')*1000 + (dateStr[1]-'0')*100 +
                (dateStr[2]-'0')*10 + (dateStr[3]-'0');
        month = (dateStr[5]-'0')*10 + (dateStr[6]-'0');
        day = (dateStr[8]-'0')*10 + (dateStr[9]-'0');

        hour = (dateStr[11]-'0')*10 + (dateStr[12]-'0');
        min = (dateStr[14]-'0')*10 + (dateStr[15]-'0');
        sec = (dateStr[17]-'0')*10 + (dateStr[18]-'0');
        
        return true;
}

double CObsMetadata::parseFitsDateToMJD(const char* valueStr)
{
        int year, month, day, hour, min;
        double sec;
        parseFitsDate(valueStr, year, month, day, hour, min, sec);

// INFO/TODO : either way below is OK, for now I am leaving it the same as in COTTER (using pal):

// WARNING : has to be MJD = (JD-2400000.5) -> my function date2jd is not correct, unless changed to JD-2400000.5
// 	
	return 0; // CRISTIAN
        // return PacerGeometry::GetMJD(year, month, day, hour, min, sec);

// Wrong - should be truncated (JD-2400000.5)
// this is ok TOO :
//        return ::date2jd( year, month, day, hour, min, sec ) - 2400000.5; 
}

bool CObsMetadata::parseIntArray(const char* valueStr, int* values, size_t count)
{
        // printf("DEBUG : valueStr = %s , count = %d\n",valueStr,int(count));
        std::string str;
        if( !parseFitsString(valueStr,str) ){
           return false;
        }
        size_t pos = 0;
        for(size_t i=0; i!=count-1; ++i)
        {
                size_t next = str.find(',', pos);
                if(next == str.npos){
                        //printf("DEBUG : next = %d , pos =  %d\n",int(next),int(str.npos));
                        // printf("ERROR : parsing integer list %s in metafits file\n",valueStr);
                        return false;
                }
                *values = atoi(str.substr(pos, next-pos).c_str());
                ++values;
                pos = next+1;
        }
        *values = atoi(str.substr(pos).c_str());
        
        return true;
}

bool CObsMetadata::parseBool(const char* valueStr)
{
        if(valueStr[0] != 'T' && valueStr[0] != 'F'){
                // throw std::runtime_error("Error parsing boolean in fits file");
                printf("ERROR : could not parse bool string %s\n",valueStr);
        }
        return valueStr[0]=='T';
}

double CObsMetadata::GetDateFirstScanFromFields() const
{
//        return 0.5*(integrationTime/86400.0) + Geometry::GetMJD( year, month, day, refHour, refMinute, refSecond);
   return -1;
}

int CObsMetadata::ReadAntPositions()
{
   if( !_fptr ){
      printf("ERROR : in code FITS file not opened\n");
      return -1;
   }

   int status = 0;
   bool gotTileName = true;    // True if the 'TileName' column exists

   int hduType;
   fits_movabs_hdu(_fptr, 2, &hduType, &status);
   checkStatus(status,"Could not open list of tiles");
   
   
   char
      inputColName[] = "Input",
      antennaColName[] = "Antenna",
      tileColName[] = "Tile",
      tilenameColName[] = "TileName",
      polColName[] = "Pol",
      rxColName[] = "Rx",
      slotColName[] = "Slot",
      flagColName[] = "Flag",
      lengthColName[] = "Length",
      eastColName[] = "East",
      northColName[] = "North",
      heightColName[] = "Height",
      gainsColName[] = "Gains";
   int inputCol, antennaCol, tileCol, tilenameCol, polCol, rxCol, slotCol, flagCol, lengthCol, northCol, eastCol, heightCol, gainsCol;

   fits_get_colnum(_fptr, CASESEN, inputColName, &inputCol, &status);
   fits_get_colnum(_fptr, CASESEN, antennaColName, &antennaCol, &status);
   fits_get_colnum(_fptr, CASESEN, tileColName, &tileCol, &status);
   checkStatus(status, "Could not read column names for list of tiles");

   // The tile name column doesn't exist in older metafits files
   fits_get_colnum(_fptr, CASESEN, tilenameColName, &tilenameCol, &status);
   if (status != 0)
   {
      gotTileName = false;
      status = 0;
   }

   fits_get_colnum(_fptr, CASESEN, polColName, &polCol, &status);
   fits_get_colnum(_fptr, CASESEN, rxColName, &rxCol, &status);
   fits_get_colnum(_fptr, CASESEN, slotColName, &slotCol, &status);
   fits_get_colnum(_fptr, CASESEN, flagColName, &flagCol, &status);
   fits_get_colnum(_fptr, CASESEN, lengthColName, &lengthCol, &status);
   fits_get_colnum(_fptr, CASESEN, eastColName, &eastCol, &status);
   fits_get_colnum(_fptr, CASESEN, northColName, &northCol, &status);
   fits_get_colnum(_fptr, CASESEN, heightColName, &heightCol, &status);
   checkStatus(status,"Could not read column names for list of tiles (PART2)");

   // The gains col is not present in older metafits files
   fits_get_colnum(_fptr, CASESEN, gainsColName, &gainsCol, &status);
   if(status != 0)
   {
      gainsCol = -1;
      status = 0;
   }

   long int nrow;
   fits_get_num_rows(_fptr, &nrow, &status);
   checkStatus(status,"Could not get number of rows");

   antenna_positions.resize(nrow/2); // was antennae.resize(nrow/2);
//   inputs.resize(nrow);
   input_mapping.resize(nrow);
   for(long int i=0; i!=nrow; ++i)
   {
      int input, antenna, tile, rx, slot, flag, gainValues[24];
      double north, east, height;
      char pol;
      char length[81] = "";
      char *lengthPtr[1] = { length };
      char tilename[81] = "";
      char *tilenamePtr[1] = { tilename };
      fits_read_col(_fptr, TINT, inputCol, i+1, 1, 1, 0, &input, 0, &status);
      fits_read_col(_fptr, TINT, antennaCol, i+1, 1, 1, 0, &antenna, 0, &status);
      fits_read_col(_fptr, TINT, tileCol, i+1, 1, 1, 0, &tile, 0, &status);
      if (gotTileName){
          fits_read_col(_fptr, TSTRING, tilenameCol, i+1, 1, 1, 0, tilenamePtr, 0, &status);
      }

      fits_read_col(_fptr, TBYTE, polCol, i+1, 1, 1, 0, &pol, 0, &status);
      fits_read_col(_fptr, TINT, rxCol, i+1, 1, 1, 0, &rx, 0, &status);
      fits_read_col(_fptr, TINT, slotCol, i+1, 1, 1, 0, &slot, 0, &status);
      fits_read_col(_fptr, TINT, flagCol, i+1, 1, 1, 0, &flag, 0, &status);
      fits_read_col(_fptr, TSTRING, lengthCol, i+1, 1, 1, 0, lengthPtr, 0, &status);
      fits_read_col(_fptr, TDOUBLE, eastCol, i+1, 1, 1, 0, &east, 0, &status);
      fits_read_col(_fptr, TDOUBLE, northCol, i+1, 1, 1, 0, &north, 0, &status);
      fits_read_col(_fptr, TDOUBLE, heightCol, i+1, 1, 1, 0, &height, 0, &status);
      if(gainsCol != -1)
         fits_read_col(_fptr, TINT, gainsCol, i+1, 1, 24, 0, gainValues, 0, &status);
      checkStatus(status,"Could not read tile");
      input_mapping[input] = 2 * antenna + (pol == 'X' ? 0 : 1);
      
      if(pol == 'X'){
          InputMapping& ant = antenna_positions[antenna]; // was MWAAntenna &ant = antennae[antenna];
          if (gotTileName){
             tilename[80] = 0;
             ant.szAntName = tilename;
          }else{
             char szTmp[64];
             sprintf(szTmp,"Tile%03d",tile);
             ant.szAntName = szTmp;
          }

         ant.antenna = antenna; // CRISTIAN tile;
		 ant.input = input;
         ant.x = east;
         ant.y = north;
         ant.z = height;
         // CRISTIAN PacerGeometry::ENH2XYZ_local(east, north, height, geo_lat*(M_PI/180.00), ant.x, ant.y, ant.z); // was MWAConfig::ArrayLattitudeRad()
//         ant.stationIndex = antenna;
         // PRINTF_DEBUG("DEBUG antenna positions : %d = %s : (%.4f,%.4f,%.4f) -> (%.4f,%.4f,%.4f)\n",antenna,ant.szAntName.c_str(), east, north, height, ant.x, ant.y, ant.z );
         //std::cout << ant.name << ' ' << input << ' ' << antenna << ' ' << east << ' ' << north << ' ' << height << '\n';
       }else{
          if(pol != 'Y'){
             //throw std::runtime_error("Error parsing polarization");
             printf("ERROR : polarisation cannot be Y");
          }
       }  

       length[80] = 0;
       std::string lengthStr = length;
/*       if(input >= nrow){
          std::ostringstream errstr;
          errstr << "Tile " << tile << " has an input with index " << input << ", which is beyond the maximum index of " << (nrow-1);
          throw std::runtime_error(errstr.str());
       }
       inputs[input].inputIndex = input;
       inputs[input].antennaIndex = antenna;
       if(lengthStr.substr(0, 3) == "EL_")
          inputs[input].cableLenDelta = atof(&(lengthStr.c_str()[3]));
       else
          inputs[input].cableLenDelta = atof(lengthStr.c_str()) * MWAConfig::VelocityFactor();
       inputs[input].polarizationIndex = (pol=='X') ? 0 : 1;
       inputs[input].isFlagged = (flag!=0);
       if(gainsCol != -1) {
          for(size_t sb=0; sb!=24; ++sb) {
            // The digital pfb gains are multiplied with 64 to allow more careful
            // fine tuning of the gains.
            inputs[input].pfbGains[sb] = (double) gainValues[sb] / 64.0;
          }
       }     */
   }   
  

// continue as in /home/msok/github/pacer/software/cotter/metafitsfile.cpp

   return 0;
}

}

std::vector<int> read_metafits_mapping(const std::string& filename){
   ::CObsMetadata meta;
	if(!meta.ReadMetaData(filename.c_str())){
      std::cerr << "impossible to read metadata file." << std::endl;
      throw std::exception();
   }
	std::vector<int> mapping = meta.input_mapping;
   return mapping;
}


ObservationInfo read_obsinfo(const std::string& filename){
   ::CObsMetadata meta;
	if(!meta.ReadMetaData(filename.c_str())){
      std::cerr << "impossible to read metadata file." << std::endl;
      throw std::exception();
   }
   ObservationInfo info;
   info.nAntennas;
   info.nFrequencies;
   info.nPolarizations;
   info.nTimesteps;
   info.timeResolution;
}
