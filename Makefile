SRC = main.cc
OBJ = $(SRC:.cc=.o)
CXXFLAGS := -Wall -Werror -Wpedantic -Wextra -std=c++11
LDLIBS += -lhidapi-libusb
$(info $(SRC) => $(OBJ) )

smartdroid: $(OBJ)
	$(CXX) $(CXXFLAGS)  $< -o $@ $(LDLIBS)

clean:
	$(RM) -rf $(OBJ) smartdroid
