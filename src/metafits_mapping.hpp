#ifndef _METAFITS_MAPPING_H__
#define _METAFITS_MAPPING_H__

#include <string>
#include <vector>
#include "astroio.hpp"
std::vector<int> read_metafits_mapping(const std::string& filename);
ObservationInfo read_obsinfo(const std::string& filename);
#endif
