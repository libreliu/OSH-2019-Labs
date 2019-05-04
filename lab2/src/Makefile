.PHONY: clean

CC = g++ -g
CXX = g++ -g

yash: y.tab.c lex.yy.c shell.o pt.o executor.o
	$(CC) -o y.tab.o -c y.tab.c
	$(CC) -o lex.yy.o -c lex.yy.c
	$(CXX) -o yash y.tab.o lex.yy.o shell.o pt.o executor.o

shell.o: shell.cpp pt.hpp executor.hpp
	$(CXX) -c shell.cpp -o shell.o

pt.o: pt.cpp pt.hpp
	$(CXX) -c pt.cpp -o pt.o

executor.o: executor.cpp executor.hpp
	$(CXX) -c executor.cpp -o executor.o

y.tab.c: yash.l yash.y pt.hpp
	yacc -d yash.y

lex.yy.c: yash.l pt.hpp
	lex --header-file=lex.h yash.l

clean:
	@echo "cleanning project"
	-rm lex.yy.c y.tab.c y.tab.h *.o
	-rm y.output
	-rm yash
	-rm lex.h
	@echo "done"