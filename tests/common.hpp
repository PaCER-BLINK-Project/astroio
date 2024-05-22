#ifndef __COMMON_H__
#define __COMMON_H__

#include <exception>
#include <string>

#define ENV_DATA_ROOT_DIR "BLINK_TEST_DATADIR"

class TestFailed : public std::exception {
    private:
    std::string message;

    public:
    explicit TestFailed(std::string msg) : message {msg} {}
    virtual const char* what() const noexcept {return message.c_str();}
};


#endif