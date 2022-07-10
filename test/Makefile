CFLAGS := -fPIE -ftrapv -fstack-protector-all -Wformat-security -D_FORTIFY_SOURCE=2 -O2 -g -Wall -Werror -fcf-protection=full -I../lib
CC := gcc
NAME := iommu
SRCS_APP = test_app.c
OBJ_APP = $(SRCS_APP:.c=.o)
iommu: $(OBJ_APP)
	$(CC) -static -o $@ $^ $(CFLAGS) ../lib/lib$(NAME).a

clean:
	$(RM) *.o *.so* iommu tags log
