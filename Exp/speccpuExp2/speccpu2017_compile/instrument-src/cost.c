int _start(unsigned long ptr){
unsigned long *att_arr;
unsigned long ret;
long enc_scheme;
unsigned long masked_ptr;
unsigned int att_index;
unsigned long gtt_index;
unsigned long *att_real_start;
unsigned long new_ptr;

if (!ptr)
    return 0;
ret = ptr;
enc_scheme = (long)ptr;
if (enc_scheme < 0) {
        masked_ptr = ptr & 0xfffffffffff;
        att_index = (masked_ptr >> 4) & 0xfffffff;
        masked_ptr = masked_ptr >> 32;
        gtt_index = masked_ptr;
        att_real_start = (unsigned long *)(att_arr[gtt_index]);
        new_ptr = att_real_start[att_index];
        ret = new_ptr;
}
return ret;
}

