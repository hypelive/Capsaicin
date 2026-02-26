/**********************************************************************
Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/

#ifndef MIS_HLSL
#define MIS_HLSL

#define MIS_BALANCE 0  /**< Use balance heuristic */
#define MIS_POWER   1  /**< Use power heuristic */

#ifndef MIS_HEURISTIC
#   define MIS_HEURISTIC MIS_BALANCE
#endif

/**
 * Balanced heuristic used in MIS weight calculation.
 * @param fPDF The PDF of the sampled value.
 * @param gPDF The PDF of the MIS weighting value.
 * @return The calculated weight.
 */
float balanceHeuristic(float fPDF, float gPDF)
{
    return fPDF / (fPDF + gPDF);
}

/**
 * Balanced heuristic used in MIS weight calculation over 3 input functions.
 * @param fPDF The PDF of the sampled value.
 * @param gPDF The PDF of the MIS weighting value.
 * @param hPDF The PDF of the second MIS weighting value.
 * @return The calculated weight.
 */
float balanceHeuristic(float fPDF, float gPDF, float hPDF)
{
    return fPDF / (fPDF + gPDF + hPDF);
}

/**
 * Power heuristic used in MIS weight calculation.
 * @param fPDF The PDF of the sampled value.
 * @param gPDF The PDF of the MIS weighting value.
 * @return The calculated weight.
 */
float powerHeuristic(float fPDF, float gPDF)
{
    return (fPDF * fPDF) / (fPDF * fPDF + gPDF * gPDF);
}

/**
 * Power heuristic used in MIS weight calculation over 3 input functions.
 * @param fPDF The PDF of the sampled value.
 * @param gPDF The PDF of the MIS weighting value.
 * @param hPDF The PDF of the second MIS weighting value.
 * @return The calculated weight.
 */
float powerHeuristic(float fPDF, float gPDF, float hPDF)
{
    return (fPDF * fPDF) / (fPDF * fPDF + gPDF * gPDF + hPDF * hPDF);
}

/**
 * Heuristic used in MIS weight calculation.
 * @param fPDF The PDF of the sampled value.
 * @param gPDF The PDF of the MIS weighting value.
 * @return The calculated weight.
 */
float heuristicMIS(float fPDF, float gPDF)
{
#if MIS_HEURISTIC == MIS_BALANCE
    return balanceHeuristic(fPDF, gPDF);
#else
    return powerHeuristic(fPDF, gPDF);
#endif
}

/**
 * Heuristic used in MIS weight calculation over 3 input functions.
 * @param fPDF The PDF of the sampled value.
 * @param gPDF The PDF of the MIS weighting value.
 * @param hPDF The PDF of the second MIS weighting value.
 * @return The calculated weight.
 */
float heuristicMIS(float fPDF, float gPDF, float hPDF)
{
#if MIS_HEURISTIC == MIS_BALANCE
    return balanceHeuristic(fPDF, gPDF, hPDF);
#else
    return powerHeuristic(fPDF, gPDF, hPDF);
#endif
}

#endif // MIS_HLSL
