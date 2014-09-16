#include <iostream>
#include <hidapi/hidapi.h>
#include <tclap/CmdLine.h>

//USB identifiers
const int vendor = 0x04d8;
const int product = 0x003f;

enum class Command {
    RequestData      = 0x37,
    RequestStartStop = 0x80,
    RequestStatus    = 0x81,
    RequestOnOff     = 0x82,
    RequestVersion   = 0x83,
};

template <typename T>
class OneOf : public TCLAP::Constraint<T>
{
    std::vector<T> strs;

    virtual bool check(const T& value) const override {
        for (auto i : strs)
            if (value == i) return true;
        return false;
    }

    virtual std::string description() const override {
        std::string res;
        for (auto i : strs) res += (i + "|");
        res.resize(res.size()-1);
        return res;
    }

    virtual std::string shortID() const override {
        return description();
    }
public:
    OneOf(std::vector<T>&& lst) : strs(std::move(lst)) {}
};

//returns -1  if no devices were found, else 0
int listDevices();
int readValue(const std::string& m, const std::string& device);
int performAction(const std::string& m, const std::string& device);

int main(int argc, char** argv) 
{
    try {
        TCLAP::CmdLine cmd("Odroid SmartPower client", ' ', "0.1");
        TCLAP::ValueArg<std::string> device("d","device","Device to use", false, "", "usb-id", cmd);
        TCLAP::ValueArg<std::string> meter("m","metric","Metric to output",false,"",new OneOf<std::string>({"voltage","power","current","energy","measure","on","raw"}));
        TCLAP::ValueArg<std::string> action("a","action","Perform action",false,"",new OneOf<std::string>({"on","off","start","stop"}));
        TCLAP::SwitchArg list("l","list","Lists available devices",false);

        std::vector<TCLAP::Arg*> lst {&meter, &action, &list};
        cmd.xorAdd(lst);
        cmd.parse(argc,argv);
    
        if (list.getValue()) return listDevices();

        if (!meter.getValue().empty()) 
            return readValue(meter.getValue(),device.getValue());
        if (!action.getValue().empty()) 
            return performAction(action.getValue(),device.getValue());
        
        std::cerr << "You must either specify a metric to read o an action!" 
                  << std::endl << "Please use -m or -a" << std::endl;
        return -2;
        
    } catch (TCLAP::ArgException &e) { 
        std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl; 
    }
}

void sendCmd(hid_device* dev, Command cmd) 
{
    unsigned char buf[65] = {'\0'};
    buf[1] = static_cast<unsigned char>(cmd);
    
    if (hid_write(dev,buf,65) == -1) {
        std::cerr << "Error writing to USB device!" << std::endl;
    }
}

bool smartRead(hid_device* dev, unsigned char* buf, size_t size) 
{
    size_t read = 0;
    while (read < size) {
        int tmp = hid_read(dev,buf+read,size-read);
        if (tmp < 0) return false;
        read += tmp;
    }

    return true;
}

int performAction(const std::string& action, const std::string& device) 
{
    hid_device* dev = hid_open_path(device.c_str());
    if (dev == nullptr) {
        std::cerr << "Could not open device " << device;
        return -1;
    }

    unsigned char status[65];
    sendCmd(dev,Command::RequestStatus);
    
    if (!smartRead(dev,status,64)) {
        std::cerr << "Could not read device " << device;
        return -3;
    }

    if ((action == "on"  && status[2] != 1) ||
        (action == "off" && status[2] == 1)) {
        sendCmd(dev,Command::RequestOnOff);
        return 0;
    } else if ((action == "start" && status[1] != 1) ||
                (action == "stop" && status[1] == 1)) {
        sendCmd(dev,Command::RequestStartStop);
        return 0;
    }
    
    return -1;
}

float bufToFloat(char* buf, int start, int len) 
{
    std::stringstream str(std::string(buf+start,len));
    float val;
    str >> val;
    return val;
}

int readValue(const std::string& m, const std::string& device) 
{
    hid_device* dev = hid_open_path(device.c_str());
    if (dev == nullptr) {
        std::cerr << "Could not open device " << device;
        return -2;
    }

    unsigned char buf[65] = {'\0'};
    char* b2 = reinterpret_cast<char*>(buf);
    if (m == "on" || m == "measure") {
        sendCmd(dev,Command::RequestStatus);
    } else {
        sendCmd(dev,Command::RequestData);
    }
    
    if (!smartRead(dev,buf,64)) {
        std::cerr << "Could not read device " << device;
        return -3;
    }
    
    hid_close(dev);
    if (m == "raw") {
        for (auto c : buf) {
            if (c < 0x20 || c > 127 || c == 134) {
                std::cout << "\\" << std::hex << static_cast<unsigned>(c) << std::dec;
            } else {
                std::cout << c;
            }
        }
    } else if (m == "on") {
        std::cout << (buf[2] == 1) << std::endl;
        return (buf[2] == 1);
    } else if (m == "measure") {
        std::cout << (buf[1] == 1) << std::endl;
        return (buf[1] == 1);
    } 
    else if (m == "voltage") std::cout << bufToFloat(b2,2,5); 
    else if (m == "current") std::cout << bufToFloat(b2,11,5);
    else if (m == "power")   std::cout << bufToFloat(b2,17,6);
    else if (m == "energy")  std::cout << bufToFloat(b2,24,9);
    else  __builtin_unreachable();

    std::cout << std::endl;
    return 0;
}

int listDevices() 
{
    std::cout << "Listing devices:" << std::endl;
    std::cout << "================" << std::endl << std::endl;

    hid_device_info *info = hid_enumerate(vendor,product);
    auto first = info;
    if (info == nullptr) return -1;

    do {
        std::wcout << info->path << " " << info->manufacturer_string << " " << info->product_string << std::endl;
    } while ( (info = info->next) );

    hid_free_enumeration(first);
    return 0;
}
