
/*
 * Copyright (C) 2001
 * Ferdinando Ametrano, Luigi Ballabio, Adolfo Benin, Marco Marchioro
 *
 * This file is part of QuantLib.
 * QuantLib is a C++ open source library for financial quantitative
 * analysts and developers --- http://quantlib.sourceforge.net/
 *
 * QuantLib is free software and you are allowed to use, copy, modify, merge,
 * publish, distribute, and/or sell copies of it under the conditions stated
 * in the QuantLib License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
 *
 * You should have received a copy of the license along with this file;
 * if not, contact ferdinando@ametrano.net
 *
 * QuantLib license is also available at
 *   http://quantlib.sourceforge.net/LICENSE.TXT
*/

/*! \file multiperiodoption.cpp
    \brief base class for option with events happening at different periods

    $Source$
    $Log$
    Revision 1.7  2001/04/05 07:57:46  marmar
    One bug fixed in bermudan option, theta, rho, and vega  still not working

    Revision 1.6  2001/04/04 12:13:24  nando
    Headers policy part 2:
    The Include directory is added to the compiler's include search path.
    Then both your code and user code specifies the sub-directory in
    #include directives, as in
    #include <Solvers1d/newton.hpp>

    Revision 1.5  2001/04/04 11:07:24  nando
    Headers policy part 1:
    Headers should have a .hpp (lowercase) filename extension
    All *.h renamed to *.hpp

    Revision 1.4  2001/04/03 17:05:33  marmar
    Error messages are now more clear

    Revision 1.3  2001/03/21 11:31:55  marmar
    Main loop tranfered from method value to method calculate.
    Methods vega and rho belong now to class BSMOption

    Revision 1.2  2001/03/21 10:48:08  marmar
    valueAtCenter, firstDerivativeAtCenter, secondDerivativeAtCenter,
    are no longer methods of BSMNumericalOption but separate
    functions

    Revision 1.1  2001/03/20 15:13:43  marmar
    MultiPeriodOption is a generalization of DividendAmericanOption

*/

#include "Pricers/americancondition.hpp"
#include "Pricers/multiperiodoption.hpp"
#include "FiniteDifferences/valueatcenter.hpp"

namespace QuantLib {

    namespace Pricers {
        using FiniteDifferences::valueAtCenter;
        using FiniteDifferences::firstDerivativeAtCenter;
        using FiniteDifferences::secondDerivativeAtCenter;

        MultiPeriodOption::MultiPeriodOption(Type type, double underlying,
            double strike, Rate dividendYield, Rate riskFreeRate,
            Time residualTime, double volatility,
            const std::vector<Time>& dates, 
            int timeSteps, int gridPoints)
        : dates_(dates), 
          dateNumber_(dates.size()),
          timeStepPerPeriod_(timeSteps),
          BSMNumericalOption(type, underlying, strike, dividendYield, 
          riskFreeRate, residualTime, volatility, gridPoints) {

            if (dateNumber_ > 0){
                QL_REQUIRE(dates_[0] > 0, "First date be positive");
                QL_REQUIRE(dates_[dates_.size()-1] < residualTime,
                    "dates must be within the residual time");

                if (dateNumber_ > 0){
                    for (unsigned int j = 1; j < dateNumber_; j++)
                        QL_REQUIRE(dates_[j-1] < dates_[j],
                            "Dates must be in increasing order");
                }

            }

        }

        void MultiPeriodOption::calculate() const {

            Time beginDate, endDate;
            setGridLimits();
            initializeGrid();
            initializeInitialCondition();
            initializeOperator();
            prices_ = initialPrices_;
            controlPrices_ = initialPrices_;

            double dt = 0.01;
/*            if (dateNumber_ > 0)                
                dt = residualTime_/(timeStepPerPeriod_*dateNumber_*10);
            else
                dt = residualTime_/(timeStepPerPeriod_*10);*/
                
            
            
            int j = dateNumber_ - 1;
            do{
                initializeStepCondition();
                initializeModel();

                if (j == dateNumber_ - 1)
                    beginDate = residualTime_;
                else
                    beginDate = dates_[j+1];

                if (j >= 0)
                    endDate = dates_[j];
                else
                    endDate = dt;

                model_ -> rollback(prices_, beginDate, endDate,
                                  timeStepPerPeriod_, stepCondition_);

                model_ -> rollback(controlPrices_, beginDate, endDate,
                                  timeStepPerPeriod_);

                if (j >= 0)
                    executeIntermediateStep(j);
            } while (--j >= -1);

            double valuePlus = valueAtCenter(prices_) -
                               valueAtCenter(controlPrices_) +
                               analitic_ -> value();

            model_ -> rollback(controlPrices_, dt, 0, 1);
            model_ -> rollback(prices_,        dt, 0, 1, stepCondition_);

            // Option price and greeks are computed
            value_ = valueAtCenter(prices_) -
                     valueAtCenter(controlPrices_) + analitic_ -> value();

            delta_ = firstDerivativeAtCenter(prices_, grid_) -
                     firstDerivativeAtCenter(controlPrices_, grid_) +
                     analitic_ -> delta();

            gamma_ = secondDerivativeAtCenter(prices_, grid_) -
                     secondDerivativeAtCenter(controlPrices_, grid_) +
                     analitic_ -> gamma();

            // calculating theta_
            model_ -> rollback(controlPrices_, 0, -dt, 1);
            model_ -> rollback(prices_,        0, -dt, 1, stepCondition_);

            double valueMinus = valueAtCenter(prices_) -
                                valueAtCenter(controlPrices_) +
                                analitic_ -> value();

            theta_ = (valuePlus - valueMinus)/(2.0*dt);
            hasBeenCalculated_ = true;
        }

        using FiniteDifferences::StandardStepCondition;

        void MultiPeriodOption::initializeStepCondition() const{
            stepCondition_ = Handle<StandardStepCondition> (
                             new AmericanCondition(initialPrices_));
        }

        using FiniteDifferences::StandardFiniteDifferenceModel;

        void MultiPeriodOption::initializeModel() const{
            model_ = Handle<StandardFiniteDifferenceModel> (
                     new StandardFiniteDifferenceModel
                     (finiteDifferenceOperator_));
        }

    }

}
