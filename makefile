#format is target-name: target dependencies
#{-tab-}actions

# All Targets
all: myElf

# Tool invocations
# Executable "hello" depends on the files hello.o and myElf.o.
myElf: myElf.o 
	gcc -g -m32 -Wall -o myElf myElf.o 

# Depends on the source and header files
myElf.o: task3.c 
	gcc -m32 -g -Wall -c -o myElf.o task3.c 
 
#tell make that "clean" is not a file name!
.PHONY: clean

#Clean the build directory
clean: 
	rm -f *.o myElf
