CC      = gcc
CFLAGS  = -Wall -Wno-unused-function -g
TARGET  = celestra

all: $(TARGET)

parser.tab.c parser.tab.h: parser.y
	bison -d -o parser.tab.c parser.y

lex.yy.c: lexer.l parser.tab.h
	flex -o lex.yy.c lexer.l

$(TARGET): parser.tab.c lex.yy.c ast.c interpreter.c
	$(CC) $(CFLAGS) -o $(TARGET) parser.tab.c lex.yy.c ast.c interpreter.c -lm -lfl

clean:
	rm -f $(TARGET) parser.tab.c parser.tab.h lex.yy.c

.PHONY: all clean
