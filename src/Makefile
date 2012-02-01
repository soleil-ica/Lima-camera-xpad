xpad-objs = XpadCamera.o XpadInterface.o

SRCS = $(xpad-objs:.o=.cpp) 

CXXFLAGS += -I../include -I../../../hardware/include -I../../../common/include \
			-I../../../third-party/yat/include \
			-I/home/xpix_user/PCI_VALIDATED/trunk/sw/xpci_lib \
			-Wall -pthread -fPIC -g

all:	Xpad.o

Xpad.o:	$(xpad-objs)
	$(LD) -o $@ -r $+

clean:
	rm -f *.o *.P

%.o : %.cpp
	$(COMPILE.cpp) -MD $(CXXFLAGS) -o $@ $<
	@cp $*.d $*.P; \
	sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
	-e '/^$$/ d' -e 's/$$/ :/' < $*.d >> $*.P; \
	rm -f $*.d

-include $(SRCS:.cpp=.P)
