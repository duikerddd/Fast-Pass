.PHONY:main
main:main.cpp
	g++ -std=c++0x $^ -o $@ -lboost_system -lboost_filesystem -lboost_thread -lpthread  
