#include <cstdint>
#include <cmath>

extern "C" {

// Basic operations
__declspec(dllexport) void ezmath_add_f32(const float* a, const float* b, float* out, int size) {
    // OpenMP could be used here for massive arrays, but standard loops are fine for now
    for (int i = 0; i < size; ++i) {
        out[i] = a[i] + b[i];
    }
}

__declspec(dllexport) void ezmath_sub_f32(const float* a, const float* b, float* out, int size) {
    for (int i = 0; i < size; ++i) {
        out[i] = a[i] - b[i];
    }
}

__declspec(dllexport) void ezmath_mul_f32(const float* a, const float* b, float* out, int size) {
    for (int i = 0; i < size; ++i) {
        out[i] = a[i] * b[i];
    }
}

__declspec(dllexport) void ezmath_div_f32(const float* a, const float* b, float* out, int size) {
    for (int i = 0; i < size; ++i) {
        out[i] = a[i] / b[i];
    }
}

// Dot product
__declspec(dllexport) double ezmath_dot_f32(const float* a, const float* b, int size) {
    double sum = 0.0;
    for (int i = 0; i < size; ++i) {
        sum += static_cast<double>(a[i]) * static_cast<double>(b[i]);
    }
    return sum; // returns double so it fits nicely in EZ's float return
}

// Sum
__declspec(dllexport) double ezmath_sum_f32(const float* a, int size) {
    double sum = 0.0;
    for (int i = 0; i < size; ++i) {
        sum += static_cast<double>(a[i]);
    }
    return sum;
}

// Fill
__declspec(dllexport) void ezmath_fill_f32(float* out, float val, int size) {
    for (int i = 0; i < size; ++i) {
        out[i] = val;
    }
}

// Matrix Multiplication (C = A * B)
// A is (m x k), B is (k x n), out is (m x n)
__declspec(dllexport) void ezmath_matmul_f32(const float* a, const float* b, float* out, int m, int n, int k) {
    for (int i = 0; i < m; ++i) {
        for (int j = 0; j < n; ++j) {
            float sum = 0.0f;
            for (int p = 0; p < k; ++p) {
                sum += a[i * k + p] * b[p * n + j];
            }
            out[i * n + j] = sum;
        }
    }
}

} // extern "C"
