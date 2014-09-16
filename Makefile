SRC = main.cc
OBJ = $(SRC:.cc=.o)
CXXFLAGS := -Wall -Werror -Wpedantic -Wextra -std=c++11 -lhidapi-libusb
$(info $(SRC) => $(OBJ) )

smartdroid: $(OBJ)
	$(CXX) $(CXXFLAGS)  $< -o $@

clean:
	$(RM) -rf $(OBJ) smartdroid
