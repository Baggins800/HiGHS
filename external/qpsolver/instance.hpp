#ifndef __SRC_LIB_INSTANCE_HPP__
#define __SRC_LIB_INSTANCE_HPP__

#include <vector>

#include "vector.hpp"
#include "matrix.hpp"

struct SumNum {
   double sum = 0.0;
   unsigned int num = 0;
};

struct Instance {
   unsigned int num_var = 0;
   unsigned int num_con = 0;
   double offset = 0;
   Vector c;
   Matrix Q;
   std::vector<double> con_lo;
   std::vector<double> con_up;
   Matrix A;
   std::vector<double> var_lo;
   std::vector<double> var_up;

   Instance(unsigned int nv=0, unsigned int nc=0) : num_var(nv), num_con(nc), c(Vector(nv)), Q(Matrix(nv, nv)), A(Matrix(nc, nv)) {
      
   }

   double objval(const Vector& x) {
      return c * x + 0.5 * (Q.vec_mat(x) * x);
   }

   SumNum sumnumprimalinfeasibilities(const Vector& x, const Vector& rowactivity) {
      SumNum res;
      for (unsigned int row = 0; row<num_con; row++) {
         if (rowactivity.value[row] < con_lo[row]) {
            res.sum += (con_lo[row] - rowactivity.value[row]);
            res.num++;
         } else if (rowactivity.value[row] > con_up[row]) {
            res.sum += (rowactivity.value[row] - con_up[row]);
            res.num++;
         }
      }
      for (unsigned int var = 0; var<num_var; var++) {
         if (x.value[var] < var_lo[var]) {
            res.sum += (var_lo[var] - x.value[var]);
            res.num++;
         } else if (x.value[var] > var_up[var]) {
            res.sum += (x.value[var] - var_up[var]);
            res.num++;
         }
      }
      return res;
   }
};

#endif
