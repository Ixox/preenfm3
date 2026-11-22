#ifndef WAVES_H_
#define WAVES_H_

#ifdef __cplusplus
extern "C" {
#endif

extern float sinTable[];
extern float squareTable[];
extern float sawTable[];
extern float sinPosTable[];
extern float sinOrZeroTable[];
extern float sinSquareTable[];

void initWaveTables(void);

#ifdef __cplusplus
}
#endif

#endif /* WAVES_H_ */