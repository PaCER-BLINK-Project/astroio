#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string>
#include <string.h>
#include <vector>
#include <stdexcept>

namespace {
    bool ends_with(std::string const &full_string, std::string const &ending) {
        if (full_string.length() >= ending.length()) {
            return (0 == full_string.compare(full_string.length() - ending.length(), ending.length(), ending));
        } else {
            return false;
        }
    }
}

namespace blink {

    namespace imager {

        void create_directory(const std::string& path){
            char tmp[1024];
            char *p = NULL;
            size_t len;

            snprintf(tmp, sizeof(tmp),"%s", path.c_str());
            len = strlen(tmp);
            if (tmp[len - 1] == '/')
                tmp[len - 1] = 0;
            for (p = tmp + 1; *p; p++)
                if (*p == '/') {
                    *p = 0;
                    mkdir(tmp, S_IRWXU);
                    *p = '/';
                }
            mkdir(tmp, S_IRWXU);
        }

        bool dir_exists(const std::string& path){
            DIR* dir = opendir(path.c_str());
            if (dir) {
                closedir(dir);
                return true;
            } else {
                return false;
            }
        }
        
        /**
        @brief list all the files in a directory.

        @param path: path of a directory.

        @returns a vector of string objects representing the path to all files
        in the given directory.
        */
        std::vector<std::string> list_files_in_dir(std::string path, std::string ext){
            DIR *dir;
            struct dirent *ent;
            std::vector<std::string> files;
            if((dir = opendir(path.c_str())) != NULL) {
                while((ent = readdir(dir)) != NULL) {
                    std::string fname {ent->d_name};
                    if(ext.length() > 0){
                        if(::ends_with(fname, ext))
                            files.push_back(path + "/" + fname);
                    }else{
                        files.push_back(fname);
                    }
                }
                closedir (dir);
            } else {
                throw std::runtime_error {"list_files_in_dir: error while opening the directory " + path};
            }
            return files;
        }
    }
}
