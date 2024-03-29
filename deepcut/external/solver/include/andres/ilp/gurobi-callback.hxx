#pragma once
#ifndef ANDRES_ILP_GUROBI_CALLBACK_HXX
#define ANDRES_ILP_GUROBI_CALLBACK_HXX

#include <limits>
#include <stdexcept>

#include "gurobi_c++.h"

namespace andres {
namespace ilp {

template<class T = double>
class Gurobi {
public:
    typedef T value_type;
    enum PreSolver {PRE_SOLVER_AUTO, PRE_SOLVER_PRIMAL, PRE_SOLVER_DUAL, PRE_SOLVER_NONE};
    enum LPSolver {LP_SOLVER_PRIMAL_SIMPLEX, LP_SOLVER_DUAL_SIMPLEX, LP_SOLVER_BARRIER, LP_SOLVER_SIFTING};

    class Callback
    :   public GRBCallback
    {
    public:
        typedef Gurobi<value_type> GurobiType;

        Callback(
            GurobiType& gurobi
        )
        :   gurobi_(gurobi),
            objectiveBest_(std::numeric_limits<double>::infinity()),
            objectiveBound_(-std::numeric_limits<double>::infinity())
        {}

        virtual void separateAndAddLazyConstraints() = 0;

        void callback() {
            try {
                if(where == GRB_CB_MIP) {
                    objectiveBest_ = getDoubleInfo(GRB_CB_MIP_OBJBST);
                    if(objectiveBest_ == GRB_INFINITY) {
                        objectiveBest_ = std::numeric_limits<double>::infinity();
                    }
                    objectiveBound_ = getDoubleInfo(GRB_CB_MIP_OBJBND);
                }
                else if(where == GRB_CB_MIPSOL) {
                    separateAndAddLazyConstraints();
                }
            }
            catch(const GRBException& e) {
                throw std::runtime_error(std::string("error executing Gurobi callback: ") + e.getMessage());

            }
        }

    protected:
        value_type label(const std::size_t variableIndex) {
            return getSolution(gurobi_.gurobiModel_->getVar(variableIndex));
        }

        value_type objectiveBest()
            { return objectiveBest_; }
        value_type objectiveBound()
            { return objectiveBound_; }

        template<class VariableIndexIterator, class CoefficientIterator>
        void addLazyConstraint(
            VariableIndexIterator viBegin,
            VariableIndexIterator viEnd,
            CoefficientIterator coefficient,
            const T lowerBound,
            const T upperBound
        ) {
            GRBLinExpr expression;
            for(; viBegin != viEnd; ++viBegin, ++coefficient) {
                expression += (*coefficient) * gurobi_.gurobiVariables_[static_cast<size_t>(*viBegin)];
            }
            if(lowerBound == upperBound) {
                addLazy(expression == lowerBound);
            }
            else {
                if(lowerBound != -std::numeric_limits<value_type>::infinity()) {
                    addLazy(lowerBound <= expression);
                }
                if(upperBound != std::numeric_limits<value_type>::infinity()) {
                    addLazy(expression <= upperBound);
                }
            }
        }

    private:
        GurobiType& gurobi_;
        value_type objectiveBest_;
        value_type objectiveBound_;
    };

    Gurobi();
    ~Gurobi();
    void setTimeLimit(const size_t);
    void setNumberOfThreads(const size_t);
    void setAbsoluteGap(const value_type);
    void setRelativeGap(const value_type);
    void setVerbosity(const bool);
    void setLPSolver(const LPSolver);
    void setPreSolver(const PreSolver, const int = -1);
    void initModel(const size_t, const value_type*);
    template<class Iterator>
        void setStart(Iterator);
    template<class VariableIndexIterator, class CoefficientIterator>
        void addConstraint(VariableIndexIterator, VariableIndexIterator,
                           CoefficientIterator, const value_type, const value_type);
    void setCallback(Callback&);
    void optimize();

    value_type label(const size_t) const;
    size_t numberOfThreads() const;
    value_type absoluteGap() const;
    value_type relativeGap() const;

private:
    GRBEnv gurobiEnvironment_;
    GRBModel* gurobiModel_ { nullptr };
    GRBVar* gurobiVariables_ { nullptr };
    GRBLinExpr gurobiObjective_;
    size_t nVariables_ { 0 };
};

template<class T>
inline
Gurobi<T>::Gurobi()
{
    setVerbosity(false);
}

template<class T>
Gurobi<T>::~Gurobi() {
    if (gurobiModel_ != nullptr)
        delete gurobiModel_;

    if (gurobiVariables_ != nullptr)
        delete[] gurobiVariables_;
}

template<class T>
inline void
Gurobi<T>::setTimeLimit(
    const size_t numberOfSeconds
) {
    gurobiEnvironment_.set(GRB_DoubleParam_TimeLimit, numberOfSeconds);
}

template<class T>
inline void
Gurobi<T>::setNumberOfThreads(
    const size_t numberOfThreads
) {
    gurobiEnvironment_.set(GRB_IntParam_Threads, numberOfThreads);
}

template<class T>
inline void
Gurobi<T>::setAbsoluteGap(
    const T gap
) {
    gurobiEnvironment_.set(GRB_DoubleParam_MIPGapAbs, gap);
}

template<class T>
inline void
Gurobi<T>::setRelativeGap(
    const T gap
) {
    gurobiEnvironment_.set(GRB_DoubleParam_MIPGap, gap);
}

template<class T>
inline void
Gurobi<T>::setVerbosity(
    const bool verbosity
) {
    if(verbosity) {
        gurobiEnvironment_.set(GRB_IntParam_OutputFlag, 1);
    }
    else {
        gurobiEnvironment_.set(GRB_IntParam_OutputFlag, 0);
    }
}

template<class T>
inline void
Gurobi<T>::setPreSolver(
    const PreSolver preSolver,
    const int passes
) {
    switch(preSolver) {
    case PRE_SOLVER_NONE:
        gurobiEnvironment_.set(GRB_IntParam_Presolve, 0);
        return;
    case PRE_SOLVER_AUTO:
        gurobiEnvironment_.set(GRB_IntParam_PreDual, -1);
        break;
    case PRE_SOLVER_PRIMAL:
        gurobiEnvironment_.set(GRB_IntParam_PreDual, 0);
        break;
    case PRE_SOLVER_DUAL:
        gurobiEnvironment_.set(GRB_IntParam_PreDual, 1);
        break;
    }
    gurobiEnvironment_.set(GRB_IntParam_PrePasses, passes);

    // crushing allows the solver to translate variable indices in cuts 
    // to variable indices of the pre-solved problem
    /*
    if(crush) {
        gurobiEnvironment_.set(GRB_IntParam_PreCrush, 1);
    }
    else {
        gurobiEnvironment_.set(GRB_IntParam_PreCrush, 0);
    }
    */
}

template<class T>
inline void
Gurobi<T>::setLPSolver(
    const LPSolver lpSolver
) {
    switch(lpSolver) {
    case LP_SOLVER_PRIMAL_SIMPLEX:
        gurobiEnvironment_.set(GRB_IntParam_NodeMethod, 0);
        break;
    case LP_SOLVER_DUAL_SIMPLEX:
        gurobiEnvironment_.set(GRB_IntParam_NodeMethod, 1);
        break;
    case LP_SOLVER_BARRIER:
        gurobiEnvironment_.set(GRB_IntParam_NodeMethod, 2);
        break;
    case LP_SOLVER_SIFTING:
        gurobiEnvironment_.set(GRB_IntParam_NodeMethod, 1); // dual simplex
        gurobiEnvironment_.set(GRB_IntParam_SiftMethod, 1); // moderate, 2 = aggressive
        break;
    }
}

template<class T>
inline void
Gurobi<T>::initModel(
    const size_t numberOfVariables,
    const T* coefficients
) {
    nVariables_ = numberOfVariables;
    
    if (gurobiModel_ != nullptr)
        delete gurobiModel_;

    if (gurobiVariables_ != nullptr)
        delete[] gurobiVariables_;

    gurobiModel_ = new GRBModel(gurobiEnvironment_);
    gurobiVariables_ = gurobiModel_->addVars(numberOfVariables, GRB_BINARY);
    gurobiModel_->update();
    gurobiObjective_.addTerms(coefficients, gurobiVariables_, numberOfVariables);
    gurobiModel_->setObjective(gurobiObjective_);
}

template<class T>
inline void
Gurobi<T>::setCallback(
    Callback& callback
) {
    gurobiModel_->getEnv().set(GRB_IntParam_LazyConstraints, 1);
    gurobiModel_->setCallback(&callback);
}

template<class T>
inline void
Gurobi<T>::optimize() {
    gurobiModel_->optimize();
}

template<class T>
inline T
Gurobi<T>::label(
    const size_t variableIndex
) const {
    return gurobiVariables_[variableIndex].get(GRB_DoubleAttr_X);
}

template<class T>
inline size_t
Gurobi<T>::numberOfThreads() const {
    return gurobiEnvironment_.get(GRB_IntParam_Threads);
}

template<class T>
inline T
Gurobi<T>::absoluteGap() const {
    return gurobiEnvironment_.get(GRB_DoubleParam_MIPGapAbs);
}

template<class T>
inline T
Gurobi<T>::relativeGap() const {
    return gurobiEnvironment_.get(GRB_DoubleParam_MIPGap);
}

template<class T>
template<class VariableIndexIterator, class CoefficientIterator>
void Gurobi<T>::addConstraint(
    VariableIndexIterator viBegin,
    VariableIndexIterator viEnd,
    CoefficientIterator coefficient,
    const T lowerBound,
    const T upperBound
) {
    GRBLinExpr expression;
    for(; viBegin != viEnd; ++viBegin, ++coefficient) {
        expression += (*coefficient) * gurobiVariables_[static_cast<size_t>(*viBegin)];
    }
    if(lowerBound == upperBound) {
        GRBLinExpr exact(lowerBound);
        gurobiModel_->addConstr(expression, GRB_EQUAL, exact);
    }
    else {
        if(lowerBound != -std::numeric_limits<value_type>::infinity()) {
            GRBLinExpr lower(lowerBound);
            gurobiModel_->addConstr(expression, GRB_GREATER_EQUAL, lower);
        }
        if(upperBound != std::numeric_limits<value_type>::infinity()) {
            GRBLinExpr upper(upperBound);
            gurobiModel_->addConstr(expression,GRB_LESS_EQUAL, upper);
        }
    }
}

template<class T>
template<class Iterator>
void
Gurobi<T>::setStart(
    Iterator valueIterator
)
{
    for(size_t j = 0; j < nVariables_; ++j, ++valueIterator)
        gurobiVariables_[j].set(GRB_DoubleAttr_Start, static_cast<double>(*valueIterator));
}

} // namespace ilp
} // namespace andres

#endif
