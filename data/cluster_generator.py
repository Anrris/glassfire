#!/usr/bin/env python
#
# Created by Tai, Yuan-yen on 11/17/17.
# All rights reserved.
#
import numpy as np
import pandas as pd


class NDRandClusterGenerator(object):
    def __init__(self, dim=1):
        if dim < 1:
            raise Exception("Dimension should be grater than 0.")

        self.dimension = dim
        self.point_collections = []
        self.mean_cov_collections = []
        self.cluster_count = 0
        self.data = []

    def random_symm_matrix(self, diag, offdiag):
        asymm = np.random.rand(self.dimension, self.dimension)
        asymm = np.dot(asymm, asymm.T)
        symm = (asymm + asymm.T) / 2
        (col, row) = symm.shape

        (diag_base, diag_scal) = diag
        (offdiag_base, offdiag_scal) = offdiag
        for i in range(col):
            for j in range(i + 1, row):
                flag = np.random.random_integers(0, 1, 1)
                m = 1
                if flag == 0: m = -1
                symm[i, j] = m * (offdiag_base + symm[i, j] * offdiag_scal)
                symm[j, i] = m * (offdiag_base + symm[j, i] * offdiag_scal)
            symm[i, i] = diag_base + symm[i, i] * diag_scal
        return symm

    def seed(self, mean, cov, count):
        self.mean_cov_collections.append((mean, cov))
        for elem in np.random.multivariate_normal(mean, cov, count).tolist():
            self.point_collections.append((self.cluster_count, elem))
            self.data.append(elem)
        self.cluster_count += 1

    def seed_in_range(self, mean, count, diag=0, offdiag=1):
        self.seed(mean, self.random_symm_matrix(diag, offdiag), count)

    def save_data(self, filename):
        # Save the distributed data point to file

        df = pd.DataFrame(self.data)
        df.to_csv('test.csv')

        self.data = []
        out = open(filename+".csv", 'w')
        for row in self.point_collections:
            (cluster, data) = row
            out.write(str(cluster)+" 1 ")
            for elem in data:
                out.write(str(elem) + " ")
            out.write('\n')
        out.close()

        # Save the Gaussian distribution to file
        out = open(filename+".mc", 'w')
        out.write("Dimension = "+str(self.dimension))
        for item in self.mean_cov_collections:
            out.write(">>>")
            (mean_mat, cov_mat) = item
            for mean_elem in mean_mat:
                out.write(str(mean_elem) + " ")
            out.write('\n')

            (row, col) = cov_mat.shape
            for i in range(row):
                for j in range(col):
                    out.write(str(cov_mat[i,j]) + " ")
                out.write('\n')
            out.write('\n')
        out.close()



if __name__ == "__main__":
    test = NDRandClusterGenerator(2)
    test.seed_in_range(mean=(0,   0),count=15000, diag=(3, 3), offdiag=(1, 1))
    test.seed_in_range(mean=(11,  2),count=10000, diag=(3, 3), offdiag=(1, 2))
    test.seed_in_range(mean=(3,  10),count=20000, diag=(3, 3), offdiag=(2, 1))
    test.seed_in_range(mean=(12, 12),count=10000, diag=(3, 3), offdiag=(1, 2))
    test.seed_in_range(mean=(23, 15),count=15000, diag=(3, 3), offdiag=(1, 2))
    test.seed_in_range(mean=(25, 30),count=10000, diag=(3, 3), offdiag=(1, 2))
    test.seed_in_range(mean=(22, 40),count=15000, diag=(6, 6), offdiag=(3, 2))
    test.seed_in_range(mean=(-3, 40),count=15000, diag=(20,6), offdiag=(5, 2))
    test.seed_in_range(mean=(34, 10),count=25000, diag=(2, 8), offdiag=(3, 2))
    test.seed_in_range(mean=(24, 53),count=15000, diag=(10,7), offdiag=(3, 2))
    test.seed_in_range(mean=(54,-83),count=15000, diag=(30,20), offdiag=(3, 2))



    test.save_data("rand")
