#ifndef __UTILS_H__
#define __UTILS_H__

#include <cstddef>
#include <string>


/**
 * @brief Read data from a given file.
 * 
 * @param filename [IN] path to the file to be read.
 * @param data [OUT] reference to a pointer which will store the location of 
 * an array of bytes representing the file content.
 * @param file_size [OUT] number of bytes read.
 */
void read_data_from_file(std::string filename, char*& data, size_t&file_size);



/**
 * @brief Converts a human readable timespec to the corresponding number of seconds.
 * 
 * A timespec is a sting composed of a integer part `i` followed by a unit of time `u`.
 * Valid units are 'ms', 'cs', 'ds', and 's'. It is a convenient way of expressing an
 * interval of time for humans.
 * 
 * @param spec a string containing a valid timespec.
 * @return A double representing the number of seconds expressed in the timepec.
 */
double parse_timespec(const char * const spec);


/**
 * Converts a timestamp in GPS format to a UNIX timestamp.
*/
time_t gps_to_unix(time_t gps);

#endif