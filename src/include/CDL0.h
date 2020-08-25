#ifndef CDL0_H
#define CDL0_H
#include "RcppArmadillo.h"
#include "CD.h"
#include "FitResult.h" // REMOVE: may need to include params??
#include "utils.h"


template <typename T>
class CDL0: public CD<T> {
    private:
        double thr;
        std::vector<double> * Xtr;
        unsigned int Iter;
        unsigned int ScreenSize;
        std::vector<unsigned int> Range1p;
        unsigned int NoSelectK;
    public:
        CDL0(const T& Xi, const arma::vec& yi, const Params<T>& P);
        //~CDL0(){}

        FitResult<T> Fit() final;

        double Objective(arma::vec & r, arma::sp_mat & B) final;

        bool CWMinCheck();
};

template <typename T>
CDL0<T>::CDL0(const T& Xi, const arma::vec& yi, const Params<T>& P) : CD<T>(Xi, yi, P){
    thr = sqrt(2 * this->ModelParams[0]); 
    Xtr = P.Xtr; 
    Iter = P.Iter;
    this->result.ModelParams = P.ModelParams; 
    ScreenSize = P.ScreenSize;
    this->r = *P.r; 
    Range1p.resize(this->p);
    std::iota(std::begin(Range1p), std::end(Range1p), 0);
    NoSelectK = P.NoSelectK;
    this->result.r = P.r;
}

template <typename T>
FitResult<T> CDL0<T>::Fit() {
    
    //bool SecondPass = false;
    double objective = Objective(this->r, this->B);
    
    std::vector<unsigned int> FullOrder = this->Order;
    bool FirstRestrictedPass = true;
    
    if (this->ActiveSet) {
        this->Order.resize(std::min((int) (this->B.n_nonzero + ScreenSize + NoSelectK), (int)(this->p))); // std::min(1000,Order.size())
    }
    
    bool ActiveSetInitial = this->ActiveSet;
    
    for (unsigned int t = 0; t < this->MaxIters; ++t) {
        this->Bprev = this->B;
        
        for (auto& i : this->Order) {
            double cor = matrix_column_dot(*(this->X), i, this->r);
            (*Xtr)[i] = std::abs(cor); // do abs here instead from when sorting
            double Bi = this->B[i]; // B[i] is costly
            double x = cor + Bi;
            
            if (x >= thr || x <= -thr || i < NoSelectK) {	// often false so body is not costly
                this->r += matrix_column_mult(*(this->X), i, -cor);
                this->B[i] = clamp(x, this->Lows[i], this->Highs[i]);
            } else if (Bi != 0) { 
                this->r += matrix_column_mult(*(this->X), i, Bi);
                this->B[i] = 0;
            } // do nothing if x == 0 and B[i] == 0
        }
        
        if (this->Converged()) {
            if(FirstRestrictedPass && ActiveSetInitial) {
                if (CWMinCheck()) {
                    break;
                }
                FirstRestrictedPass = false;
                this->Stabilized = false;
                this->ActiveSet = true;
                this->Order = FullOrder;
            } else {
                if (this->Stabilized == true && ActiveSetInitial) { // && !SecondPass
                    if (CWMinCheck()) {
                        break;
                    }
                    this->Order = this->OldOrder; // Recycle over all coordinates to make sure the achieved point is a CW-min.
                    //SecondPass = true; // a 2nd pass will be performed
                    this->Stabilized = false;
                    this->ActiveSet = true;
                } else {
                    break;
                }
            }
        }
        
        if (this->ActiveSet) {
            this->SupportStabilized();
        }
        
    }
    
    
    this->result.Objective = objective;
    this->result.B = this->B;
    //result.Model = this;
    *(this->result.r) = this->r; // change to pointer later
    this->result.IterNum = this->CurrentIters;
    return this->result;
}

template <typename T>
bool CDL0<T>::CWMinCheck() {
    // Get the Sc = FullOrder - Order
    std::vector<unsigned int> S;
    for(arma::sp_mat::const_iterator it = this->B.begin(); it != this->B.end(); ++it) {
        S.push_back(it.row());
    }
    
    std::vector<unsigned int> Sc;
    set_difference(
        Range1p.begin(),
        Range1p.end(),
        S.begin(),
        S.end(),
        back_inserter(Sc));
    
    bool Cwmin = true;
    for (auto& i : Sc) {
        
        double x = matrix_column_dot(*(this->X), i, this->r);
        double absx = std::abs(x);
        (*Xtr)[i] = absx; // do abs here instead from when sorting
        
        // B[i] = 0 in this case!
        if (absx >= thr) {	// often false so body is not costly
            this->r -= matrix_column_mult(*(this->X), i, x);
            this->B[i] = x;
            Cwmin = false;
        }
    }
    return Cwmin;
    
}

template <typename T>
inline double CDL0<T>::Objective(arma::vec & r, arma::sp_mat & B)   // hint inline
{
    return 0.5 * arma::dot(r, r) + this->ModelParams[0] * B.n_nonzero;
}

#endif
