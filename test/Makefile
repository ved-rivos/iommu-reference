CFLAGS := -fPIE -ftrapv -fstack-protector-all -Wformat-security -D_FORTIFY_SOURCE=2 -O0 -g -Wall -Werror -fcf-protection=full -I../libiommu/include -I../libtables/include -lgcov --coverage
CC := gcc
SRCS_APP = test_app.c
OBJ_APP = $(SRCS_APP:.c=.o)
iommu: $(OBJ_APP)
	$(CC) -static -o $@ $^ $(CFLAGS) ../libiommu/libiommu.a ../libtables/libtables.a

clean:
	$(RM) src/*.o *.a* tags log src/*.gc* src/*.gcov *.gcov
