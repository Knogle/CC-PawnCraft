/* Float arithmetic and operators adapted from third_party/pawn/include/float.inc.
 * (c) Copyright 1999, Artran, Inc.; Greg Garner (gmg@artran.com).
 * PAWN toolkit: Copyright (c) 1997-2023 CompuPhase, Apache-2.0;
 * see licenses/pawn-LICENSE.txt and licenses/pawn-NOTICE.txt in the source
 * distribution, or META-INF/licenses/ccpawn/ inside the addon JAR.
 * Local changes: include guard, no dynamic-library pragma, compact operator
 * declarations, explicit floatint declaration. Natives run inside our runner.
 */
#if defined _cc_float_included
  #endinput
#endif
#define _cc_float_included

const floatround_method: {
    floatround_round = 0,
    floatround_floor,
    floatround_ceil,
    floatround_tozero,
    floatround_unbiased
};
const anglemode: { radian = 0, degrees, grades };

native Float:float(value);
native Float:strfloat(const string[]);
native Float:floatmul(Float:left, Float:right);
native Float:floatdiv(Float:left, Float:right);
native Float:floatadd(Float:left, Float:right);
native Float:floatsub(Float:left, Float:right);
native Float:floatfract(Float:value);
native floatround(Float:value, floatround_method:method = floatround_round);
native floatint(Float:value);
native floatcmp(Float:left, Float:right);
native Float:floatsqroot(Float:value);
native Float:floatpower(Float:value, Float:exponent);
native Float:floatlog(Float:value, Float:base = 10.0);
native Float:floatsin(Float:value, anglemode:mode = radian);
native Float:floatcos(Float:value, anglemode:mode = radian);
native Float:floattan(Float:value, anglemode:mode = radian);
native Float:floatabs(Float:value);

#pragma rational Float

native Float:operator*(Float:left, Float:right) = floatmul;
native Float:operator/(Float:left, Float:right) = floatdiv;
native Float:operator+(Float:left, Float:right) = floatadd;
native Float:operator-(Float:left, Float:right) = floatsub;
native Float:operator=(value) = float;

stock Float:operator++(Float:value) return value + 1.0;
stock Float:operator--(Float:value) return value - 1.0;
stock Float:operator-(Float:value) return value ^ Float:cellmin;
stock Float:operator*(Float:left, right) return floatmul(left, float(right));
stock Float:operator/(Float:left, right) return floatdiv(left, float(right));
stock Float:operator/(left, Float:right) return floatdiv(float(left), right);
stock Float:operator+(Float:left, right) return floatadd(left, float(right));
stock Float:operator-(Float:left, right) return floatsub(left, float(right));
stock Float:operator-(left, Float:right) return floatsub(float(left), right);

stock bool:operator==(Float:left, Float:right) return floatcmp(left, right) == 0;
stock bool:operator==(Float:left, right) return floatcmp(left, float(right)) == 0;
stock bool:operator!=(Float:left, Float:right) return floatcmp(left, right) != 0;
stock bool:operator!=(Float:left, right) return floatcmp(left, float(right)) != 0;
stock bool:operator>(Float:left, Float:right) return floatcmp(left, right) > 0;
stock bool:operator>(Float:left, right) return floatcmp(left, float(right)) > 0;
stock bool:operator>(left, Float:right) return floatcmp(float(left), right) > 0;
stock bool:operator>=(Float:left, Float:right) return floatcmp(left, right) >= 0;
stock bool:operator>=(Float:left, right) return floatcmp(left, float(right)) >= 0;
stock bool:operator>=(left, Float:right) return floatcmp(float(left), right) >= 0;
stock bool:operator<(Float:left, Float:right) return floatcmp(left, right) < 0;
stock bool:operator<(Float:left, right) return floatcmp(left, float(right)) < 0;
stock bool:operator<(left, Float:right) return floatcmp(float(left), right) < 0;
stock bool:operator<=(Float:left, Float:right) return floatcmp(left, right) <= 0;
stock bool:operator<=(Float:left, right) return floatcmp(left, float(right)) <= 0;
stock bool:operator<=(left, Float:right) return floatcmp(float(left), right) <= 0;
stock bool:operator!(Float:value) return (_:value & cellmax) == 0;

forward operator%(Float:left, Float:right);
forward operator%(Float:left, right);
forward operator%(left, Float:right);
