int _start(unsigned long ptr){
unsigned long ret;
long enc_scheme;
unsigned long masked_ptr;
unsigned long *new_loc;
unsigned long new_ptr;

if (!ptr)
    return 0;
ret = ptr;
enc_scheme = (long)ptr;
if (enc_scheme < 0) {
        masked_ptr = (ptr * 3) & 0x7fffffffffffffff;
        new_loc = (unsigned long *)(masked_ptr - 16);
        new_ptr = *new_loc;
        ret = new_ptr;
}
return ret;
}

