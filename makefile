CXXFLAGS=-c \
		-g \
		-Wall \
		-std=c++11

all: main.o swu_task.o
	@echo Linking objects...
	@g++ -o test_app $^ -lpthread

%.o: %.cpp
	@echo Compiling and generating object $@ ...
	@g++ $< $(CXXFLAGS) -o $@

clean:
	@rm -rf *.o test_app
