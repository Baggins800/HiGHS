#ifndef __SRC_LIB_VECTOR_HPP__
#define __SRC_LIB_VECTOR_HPP__

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

struct Vector {
   unsigned int num_nz;
   unsigned int dim;
   std::vector<unsigned int> index;
   std::vector<double> value;

   Vector(unsigned int d) : dim(d) {
      index.resize(dim);
      value.resize(dim, 0.0);
      num_nz = 0;
   }

   Vector(const Vector& vec) : num_nz(vec.num_nz), dim(vec.dim), index(vec.index), value(vec.value) {

   }

   void reset() {
      for (unsigned int i=0; i<num_nz; i++) {
         value[index[i]] = 0;
         index[i] = 0;
      }
      num_nz = 0;
   }

   Vector& repopulate(const Vector& other) {
      reset();
      for (unsigned int i=0; i<other.num_nz; i++) {
         index[i] = other.index[i];
         value[index[i]] = other.value[index[i]];
      }
      num_nz = other.num_nz;
      return *this;
   }

   Vector& operator =(const Vector& other) {
      num_nz = other.num_nz;
      dim = other.dim;
      index = other.index;
      value = other.value;
      return *this;
   }

   static Vector& unit(unsigned int dim, unsigned int u, Vector& target) {
      target.reset();
      target.index[0] = u;
      target.value[u] = 1.0;
      target.num_nz = 1;
      return target;
   }

   static Vector unit(unsigned int dim, unsigned int u) {
      Vector vec(dim);
      vec.index[0] = u;
      vec.value[u] = 1.0;
      vec.num_nz = 1;
      return vec;
   }

   void report(std::string name="") {
      if (name != "") {
         printf("%s: ", name.c_str());
      }
      for (unsigned int i=0; i<num_nz; i++) {
         printf("[%u] %lf ", index[i], value[index[i]]);
      }
      printf("\n");
   }

   double norm2() {
      double  val = 0.0;

      for (unsigned int i=0; i<num_nz; i++) {
         val += value[index[i]] * value[index[i]];
      }

      return val;
   }

   void sanitize(double threshold = 10E-15) {
      unsigned int new_idx = 0;

      for (unsigned int i=0; i<num_nz; i++) {
         if (fabs(value[index[i]]) > threshold) {
            index[new_idx++] = index[i];
         } else {
            value[index[i]] = 0.0;
            index[i] = 0;
         }
      }
      num_nz = new_idx;
   }

   void resparsify() {
      num_nz = 0;
      for (unsigned int i=0; i<dim; i++) {
         if (value[i] != 0.0) {
            index[num_nz++] = i;
         }
      }
   }

   Vector& scale(double a) {
      for (unsigned int i=0; i<num_nz; i++) {
         value[index[i]] *= a;
      }
      return *this;
   }

   Vector& saxpy(double a, double b, const Vector& x) {
      scale(a);
      saxpy(b, x);
      return *this;
   }

   Vector& saxpy(double a, const Vector& x) {
      sanitize(0.0);
      for (unsigned int i=0; i<x.num_nz; i++) {
         if (value[x.index[i]] == 0.0) {
            index[num_nz++] = x.index[i];
         }
         value[x.index[i]] += a * x.value[x.index[i]];
      }
      resparsify();
      // sanitize(0.0);
      return *this;
   }

   // void saxpy(double a, unsigned int* idx, double* val, unsigned int nnz) {
   //    for (unsigned int i=0; i<nnz; i++) {
   //       value[idx[i]] += a * val[i];
   //    }
   //    resparsify();
   // }

   Vector operator +(const Vector& other) const {
      Vector result(dim);

      for (int i=0; i<dim; i++) {
         result.value[i] = value[i] + other.value[i];
         if (result.value[i] != 0.0) {
            result.index[result.num_nz++] = i;
         }
      }

      return result;
   }

   Vector operator -(const Vector& other) const {
      Vector result(dim);

      for (int i=0; i<dim; i++) {
         result.value[i] = value[i] - other.value[i];
         if (result.value[i] != 0.0) {
            result.index[result.num_nz++] = i;
         }
      }

      return result;
   }

   Vector operator -() const {
      Vector result(dim);

      for (int i=0; i<num_nz; i++) {
         result.index[i] = index[i];
         result.value[index[i]] = -value[index[i]];
      }
      result.num_nz = num_nz;

      return result;
   }

   Vector operator *(const double d) const {
      Vector result(dim);

      for (unsigned int i=0; i<num_nz; i++) {
         result.index[i] = index[i];
         result.value[index[i]] = d * value[index[i]];
      }
      result.num_nz = num_nz;

      return result;
   }

   double dot(const Vector& other) const {
      double dot = 0.0;
      for (unsigned int i=0; i<num_nz; i++) {
         dot += value[index[i]] * other.value[index[i]];
      }

      return dot;
   }

   double operator *(const Vector& other) const {
      return dot(other);
   }

   double dot(const unsigned int* idx, const double* val, unsigned int nnz) const {
      double dot = 0.0;
      for (unsigned int i=0; i<nnz; i++) {
         dot += value[idx[i]] * val[i];
      }

      return dot;
   }

   Vector& operator +=(const Vector& other) {
      // sanitize();
      for (unsigned int i=0; i<other.num_nz; i++) {
         // if (value[other.index[i]] == 0.0) {
         //    index[num_nz++] = other.index[i];
         // }
         value[other.index[i]] += other.value[other.index[i]];
      }
      resparsify();
      return *this;
   }

   Vector& operator *=(const double d) {
      for (unsigned int i=0; i<num_nz; i++) {
         value[index[i]] *= d;
      }

      return *this;
   }
};

#endif
