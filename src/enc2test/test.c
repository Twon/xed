/*BEGIN_LEGAL 

Copyright (c) 2019 Intel Corporation

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
  
END_LEGAL */

#include "xed-interface.h"
#include "xed-get-time.h"
#include "enc2-m64-a64/hdr/xed/xed-enc2-m64-a64.h"
#include <stdio.h>
#include <stdlib.h>
#include "xed-histogram.h"

typedef xed_uint32_t (*test_func_t)(xed_uint8_t* output_buffer);

extern test_func_t test_functions_m64_a64[];
extern char const* test_functions_m64_a64_str[];
extern const xed_iclass_enum_t test_functions_m64_a64_iclass[];

xed_state_t dstate;

xed_histogram_t histo;

static void dump(xed_uint8_t* buf, xed_uint32_t len) {
    xed_uint_t i;
    for(i=0;i<len;i++) {
        printf("%02x ",buf[i]);
    }
}

xed_uint64_t total = 0;
xed_uint_t reps = 100;
int execute_test(int test_id) {
    xed_decoded_inst_t xedd;
    xed_uint32_t enclen=0;
    xed_error_enum_t err;
    test_func_t* p = test_functions_m64_a64;
    xed_uint8_t output_buffer[2*XED_MAX_INSTRUCTION_BYTES];
    char const* fn_name = test_functions_m64_a64_str[test_id];
    xed_uint64_t t1, t2, delta;
    xed_uint_t i;

    for(i=0;i<reps;i++)    {
        t1 = xed_get_time();
        //printf("Calling test function %d\n",test_id);
        enclen = (*p[test_id])(output_buffer);
        t2 = xed_get_time();
        if (t2>t1) {
            delta = t2-t1;
            total += delta;
        }
        if (i > 3)
            xed_histogram_update(&histo, t1, t2);
    }
            
    
    // This stuff should problably move in to the individual tests so
    // that we can do more validation about the iclass and operands.
    
    if (enclen > XED_MAX_INSTRUCTION_BYTES) {
        printf("\ttest id %d ERROR: %s (%s)\n", test_id, "ENCODE TOO LONG", fn_name);
        printf("\t");
        dump(output_buffer,enclen);
        printf("\n");
        return 1;
    }
    
    
    xed_decoded_inst_zero_set_mode(&xedd, &dstate);
    // set some modern decode mode behavior
    xed3_operand_set_cet(&xedd, 1);
    xed3_operand_set_cldemote(&xedd, 1);
    xed3_operand_set_wbnoinvd(&xedd, 1);
    err = xed_decode(&xedd, output_buffer, enclen);
    if (err == XED_ERROR_NONE) {
        if (xed_decoded_inst_get_iclass(&xedd) != test_functions_m64_a64_iclass[test_id]) {
            printf("\ttest id %d ICLASS MISMATCH: observed: %s expected: %s (%s)\n", test_id,
                   xed_iclass_enum_t2str( xed_decoded_inst_get_iclass(&xedd) ),
                   xed_iclass_enum_t2str( test_functions_m64_a64_iclass[test_id] ),
                   fn_name);
            printf("\t");
            dump(output_buffer,enclen);
            printf("\n");
            return 1;
        }
    }
    else {
        printf("\ttest id %d ERROR: %s (%s)\n", test_id, xed_error_enum_t2str(err), fn_name);
        printf("\t");
        dump(output_buffer,enclen);
        printf("\n");
        return 1;
    }

    
    return 0;
}


int test_m64_a64(void) {
    xed_uint32_t test_id=0;
    xed_uint32_t errors = 0;
    test_func_t* p = test_functions_m64_a64;
    xed_uint64_t t1, t2, delta;
    

    t1 = xed_get_time();
    while(*p) {
        if (execute_test(test_id)) {
            printf("test %d failed\n", test_id);
            errors++;
        }
        p++;
        test_id++;
    }
    t2 = xed_get_time();
    delta = t2-t1;


    printf("Tests:   %6d\n", test_id);
    printf("Repeats: %6d\n", reps);
    printf("Errors:  %6d\n", errors);
    printf("Cycles: " XED_FMT_LU "\n", delta);
    printf("Cycles/(enc+dec) : %7.1lf\n", 1.0*delta/(reps*test_id));
    printf("Cycles/encode    : %7.1lf\n", 1.0*total/(reps*test_id));
    return errors;
}

int main(int argc, char** argv) {
    int i=0, m=0, test_id=0, errors=0;
    xed_tables_init();    
    xed_state_zero(&dstate);
    dstate.mmode=XED_MACHINE_MODE_LONG_64;
    //dstate.mmode=XED_MACHINE_MODE_LEGACY_32;
    //dstate.mmode=XED_MACHINE_MODE_LEGACY_16;
    //dstate.stack_addr_width=XED_ADDRESS_WIDTH_16b;
    //dstate.stack_addr_width=XED_ADDRESS_WIDTH_32b;
    
    xed_histogram_initialize(&histo);

    // count tests
    test_func_t* p = test_functions_m64_a64;
    while (*p) {
        i++;
        p++;
    }
    m = i;
    printf("Total tests %d\n",m);
    for(i=1;i<argc;i++) {
        test_id = (int)strtol(argv[i], (char **)NULL, 10);
        if (test_id >=  m) {
            printf("Test ID too large (range: 0...%d)\n",m-1);
            return 1;
        }
         
        if (execute_test(test_id)) {
            printf("test id %d failed\n", test_id);
            errors++;
        }
        else {
            printf("test id %d success\n", test_id);
        }
    }
    if (argc==1) {
        printf("Testing all...\n");
        errors = test_m64_a64(); // test all
    }
    xed_histogram_dump(&histo, 1);
    return errors>0;
}
