CXX      = clang++
CXXFLAGS = -std=c++23 -Wall -Wextra -Wpedantic -O3
LDFLAGS  =
TARGET   = rpn
SRCS     = main.cpp rpn.cpp commands.cpp bigint.cpp i18n.cpp
OBJS     = $(SRCS:.cpp=.o)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

main.o: rpn.h commands.h bigint.h i18n.h
rpn.o: rpn.h commands.h bigint.h i18n.h
commands.o: commands.h bigint.h i18n.h
bigint.o: bigint.h
i18n.o: i18n.h

.PHONY: test clean

test: $(TARGET)
	./test_rpn.sh

clean:
	rm -f $(TARGET) $(OBJS)
