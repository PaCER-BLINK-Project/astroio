#include <iostream>
#include <cstring>
#include <string>
#include "common.hpp"
#include "../src/metafits_mapping.hpp"


std::string data_root_dir;


void test_read_metafits_mapping(){
	std::string metadata_file {data_root_dir + "/mwa/1276619416/20200619163000.metafits"}; 
	auto mapping = read_metafits_mapping(metadata_file);
    if(mapping.size() != 256) throw TestFailed("'test_read_metadata' failed: number of inputs is not 256.");
	if(mapping[75] != 220){
        std::cout << "maapping[75] == " << mapping[75] << std::endl;
		throw TestFailed("'test_read_metadata' failed: input 75 not corresponding to 220.");
	}
    std::cout << "'test_read_metadata' passed." << std::endl;
}


void test_read_obsinfo(){
    std::string metadata_file {data_root_dir + "/mwax/1402778200.metafits"}; 
	auto obsinfo = read_obsinfo(metadata_file);
    
    std::cout << "'test_read_obsinfo' passed." << std::endl;
}

int main(void){
    char *pathToData {std::getenv(ENV_DATA_ROOT_DIR)};
    if(!pathToData){
        std::cerr << "'" << ENV_DATA_ROOT_DIR << "' environment variable is not set." << std::endl;
        return -1;
    }
    data_root_dir = std::string{pathToData};
    try{
        
        test_read_metafits_mapping();
        // test_read_obsinfo();
    } catch (TestFailed ex){
        std::cerr << ex.what() << std::endl;
        return 1;
    }
    
    std::cout << "All tests passed." << std::endl;
    return 0;
}
