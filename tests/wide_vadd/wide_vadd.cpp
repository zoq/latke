/**********
Copyright (c) 2018, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**********/

/*******************************************************************************
Description:
    Wide Memory Access Example using ap_uint<Width> datatype
    Description: This is vector addition example to demonstrate Wide Memory
    access of 512bit Datawidth using ap_uint<> datatype which is defined inside
    'ap_int.h' file.
*******************************************************************************/

//Including to use ap_uint<> datatype
#include <ap_int.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 64
#define DATAWIDTH 512
#define VECTOR_SIZE (DATAWIDTH / 32) // vector size is 16 (512/32 = 16)
typedef ap_uint<DATAWIDTH> uint512_dt;

//TRIPCOUNT identifier
const unsigned int c_chunk_sz = BUFFER_SIZE;
const unsigned int c_size     = VECTOR_SIZE;


template <typename T> // ap_uint or unsigned int can be used
float uint_to_float(T x) {
    union {
        unsigned int i;
        float f;
    } conv;
    conv.i=(unsigned int)x;
    return conv.f;
}

unsigned int float_to_uint(float x) {
    union {
        unsigned int i;
        float f;
    } conv;
    conv.f=x;
    return conv.i;
}


template <int W> // hardcoding in this version because float means S=32
ap_uint<W>  vadd_as_float( ap_uint<W> a,
					ap_uint<W> b) {
    // this implicitly unroll the loops
    const int S =32;
    ap_uint<W> res;
    #pragma HLS pipeline II=1
vaddloop:
    for(int i=0; i<W/S; i++) {
        res.range( S*(i+1)-1, S*i ) = float_to_uint (
               uint_to_float(a.range( S*(i+1)-1, S*i ) )
           +   uint_to_float(b.range( S*(i+1)-1, S*i ) )
           );
    }

    return res;
}



/*
    Vector Addition Kernel Implementation using uint512_dt datatype
    Arguments:
        in1   (input)     --> Input Vector1
        in2   (input)     --> Input Vector2
        out   (output)    --> Output Vector
        size  (input)     --> Size of Vector in Integer
   */
extern "C"
{
void wide_vadd(
        const uint512_dt *in1, // Read-Only Vector 1
        const uint512_dt *in2, // Read-Only Vector 2
        uint512_dt *out,       // Output Result
        int size               // Size in integer
    )
    {
#pragma HLS INTERFACE m_axi depth=8192 port=in1 max_read_burst_length=32 offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi depth=8192 port = in2 max_read_burst_length = 32 offset = slave bundle = gmem1
#pragma HLS INTERFACE m_axi depth=8192 port = out max_write_burst_length = 32 max_read_burst_length = 32 offset = slave bundle = gmem2
#pragma HLS INTERFACE s_axilite port = in1 bundle = control
#pragma HLS INTERFACE s_axilite port = in2 bundle = control
#pragma HLS INTERFACE s_axilite port = out bundle = control
#pragma HLS INTERFACE s_axilite port = size bundle = control
#pragma HLS INTERFACE s_axilite port = return bundle = control

	uint512_dt v1_local[BUFFER_SIZE]; // Local memory to store vector1
	uint512_dt v2_local[BUFFER_SIZE];  // Local memory to store vector2
	uint512_dt result_local[BUFFER_SIZE]; // Local Memory to store result

#pragma HLS stream variable = v1_local depth = 4
#pragma HLS stream variable = v2_local depth = 4
#pragma HLS stream variable = result_local depth = 4

	// Input vector size for integer vectors. However kernel is directly
	// accessing 512bit data (total 16 elements). So total number of read
	// from global memory is calculated here:
	int size_in16 = (size - 1) / VECTOR_SIZE + 1;

#pragma HLS DATAFLOW

	//burst read to local memory
	v1_rd:
		for (int j = 0; j < size_in16; j++) {
#pragma HLS pipeline
			v1_local[j] = in1[j];
			v2_local[j] = in2[j];
		}

	//perform vector addition
	v2_add:
		for (int j = 0; j < size_in16; j++) {
#pragma HLS pipeline
			auto tmpV1 = v1_local[j];
			auto tmpV2 = v2_local[j];
			result_local[j] = vadd_as_float(tmpV1, tmpV2);
		}

	//burst write to global memory
	v1_write:
		for (int j = 0; j < size_in16; j++) {
#pragma HLS pipeline
			out[j] = result_local[j];
		}
    }
}
