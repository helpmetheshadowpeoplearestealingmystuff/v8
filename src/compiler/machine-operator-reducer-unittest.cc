// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/bits.h"
#include "src/compiler/graph-unittest.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/machine-operator-reducer.h"
#include "src/compiler/typer.h"

namespace v8 {
namespace internal {
namespace compiler {

class MachineOperatorReducerTest : public GraphTest {
 public:
  explicit MachineOperatorReducerTest(int num_parameters = 2)
      : GraphTest(num_parameters), machine_(zone()) {}

 protected:
  Reduction Reduce(Node* node) {
    Typer typer(zone());
    JSGraph jsgraph(graph(), common(), &typer);
    MachineOperatorReducer reducer(&jsgraph);
    return reducer.Reduce(node);
  }

  MachineOperatorBuilder* machine() { return &machine_; }

 private:
  MachineOperatorBuilder machine_;
};


template <typename T>
class MachineOperatorReducerTestWithParam
    : public MachineOperatorReducerTest,
      public ::testing::WithParamInterface<T> {
 public:
  explicit MachineOperatorReducerTestWithParam(int num_parameters = 2)
      : MachineOperatorReducerTest(num_parameters) {}
  virtual ~MachineOperatorReducerTestWithParam() {}
};


namespace {

static const double kFloat64Values[] = {
    -V8_INFINITY,  -4.23878e+275, -5.82632e+265, -6.60355e+220, -6.26172e+212,
    -2.56222e+211, -4.82408e+201, -1.84106e+157, -1.63662e+127, -1.55772e+100,
    -1.67813e+72,  -2.3382e+55,   -3.179e+30,    -1.441e+09,    -1.0647e+09,
    -7.99361e+08,  -5.77375e+08,  -2.20984e+08,  -32757,        -13171,
    -9970,         -3984,         -107,          -105,          -92,
    -77,           -61,           -0.000208163,  -1.86685e-06,  -1.17296e-10,
    -9.26358e-11,  -5.08004e-60,  -1.74753e-65,  -1.06561e-71,  -5.67879e-79,
    -5.78459e-130, -2.90989e-171, -7.15489e-243, -3.76242e-252, -1.05639e-263,
    -4.40497e-267, -2.19666e-273, -4.9998e-276,  -5.59821e-278, -2.03855e-282,
    -5.99335e-283, -7.17554e-284, -3.11744e-309, -0.0,          0.0,
    2.22507e-308,  1.30127e-270,  7.62898e-260,  4.00313e-249,  3.16829e-233,
    1.85244e-228,  2.03544e-129,  1.35126e-110,  1.01182e-106,  5.26333e-94,
    1.35292e-90,   2.85394e-83,   1.78323e-77,   5.4967e-57,    1.03207e-25,
    4.57401e-25,   1.58738e-05,   2,             125,           2310,
    9636,          14802,         17168,         28945,         29305,
    4.81336e+07,   1.41207e+08,   4.65962e+08,   1.40499e+09,   2.12648e+09,
    8.80006e+30,   1.4446e+45,    1.12164e+54,   2.48188e+89,   6.71121e+102,
    3.074e+112,    4.9699e+152,   5.58383e+166,  4.30654e+172,  7.08824e+185,
    9.6586e+214,   2.028e+223,    6.63277e+243,  1.56192e+261,  1.23202e+269,
    5.72883e+289,  8.5798e+290,   1.40256e+294,  1.79769e+308,  V8_INFINITY};


static const int32_t kInt32Values[] = {
    -2147483647 - 1, -1914954528, -1698749618, -1578693386, -1577976073,
    -1573998034,     -1529085059, -1499540537, -1299205097, -1090814845,
    -938186388,      -806828902,  -750927650,  -520676892,  -513661538,
    -453036354,      -433622833,  -282638793,  -28375,      -27788,
    -22770,          -18806,      -14173,      -11956,      -11200,
    -10212,          -8160,       -3751,       -2758,       -1522,
    -121,            -120,        -118,        -117,        -106,
    -84,             -80,         -74,         -59,         -52,
    -48,             -39,         -35,         -17,         -11,
    -10,             -9,          -7,          -5,          0,
    9,               12,          17,          23,          29,
    31,              33,          35,          40,          47,
    55,              56,          62,          64,          67,
    68,              69,          74,          79,          84,
    89,              90,          97,          104,         118,
    124,             126,         127,         7278,        17787,
    24136,           24202,       25570,       26680,       30242,
    32399,           420886487,   642166225,   821912648,   822577803,
    851385718,       1212241078,  1411419304,  1589626102,  1596437184,
    1876245816,      1954730266,  2008792749,  2045320228,  2147483647};


static const int64_t kInt64Values[] = {
    V8_INT64_C(-9223372036854775807) - 1, V8_INT64_C(-8974392461363618006),
    V8_INT64_C(-8874367046689588135),     V8_INT64_C(-8269197512118230839),
    V8_INT64_C(-8146091527100606733),     V8_INT64_C(-7550917981466150848),
    V8_INT64_C(-7216590251577894337),     V8_INT64_C(-6464086891160048440),
    V8_INT64_C(-6365616494908257190),     V8_INT64_C(-6305630541365849726),
    V8_INT64_C(-5982222642272245453),     V8_INT64_C(-5510103099058504169),
    V8_INT64_C(-5496838675802432701),     V8_INT64_C(-4047626578868642657),
    V8_INT64_C(-4033755046900164544),     V8_INT64_C(-3554299241457877041),
    V8_INT64_C(-2482258764588614470),     V8_INT64_C(-1688515425526875335),
    V8_INT64_C(-924784137176548532),      V8_INT64_C(-725316567157391307),
    V8_INT64_C(-439022654781092241),      V8_INT64_C(-105545757668917080),
    V8_INT64_C(-2088319373),              V8_INT64_C(-2073699916),
    V8_INT64_C(-1844949911),              V8_INT64_C(-1831090548),
    V8_INT64_C(-1756711933),              V8_INT64_C(-1559409497),
    V8_INT64_C(-1281179700),              V8_INT64_C(-1211513985),
    V8_INT64_C(-1182371520),              V8_INT64_C(-785934753),
    V8_INT64_C(-767480697),               V8_INT64_C(-705745662),
    V8_INT64_C(-514362436),               V8_INT64_C(-459916580),
    V8_INT64_C(-312328082),               V8_INT64_C(-302949707),
    V8_INT64_C(-285499304),               V8_INT64_C(-125701262),
    V8_INT64_C(-95139843),                V8_INT64_C(-32768),
    V8_INT64_C(-27542),                   V8_INT64_C(-23600),
    V8_INT64_C(-18582),                   V8_INT64_C(-17770),
    V8_INT64_C(-9086),                    V8_INT64_C(-9010),
    V8_INT64_C(-8244),                    V8_INT64_C(-2890),
    V8_INT64_C(-103),                     V8_INT64_C(-34),
    V8_INT64_C(-27),                      V8_INT64_C(-25),
    V8_INT64_C(-9),                       V8_INT64_C(-7),
    V8_INT64_C(0),                        V8_INT64_C(2),
    V8_INT64_C(38),                       V8_INT64_C(58),
    V8_INT64_C(65),                       V8_INT64_C(93),
    V8_INT64_C(111),                      V8_INT64_C(1003),
    V8_INT64_C(1267),                     V8_INT64_C(12797),
    V8_INT64_C(23122),                    V8_INT64_C(28200),
    V8_INT64_C(30888),                    V8_INT64_C(42648848),
    V8_INT64_C(116836693),                V8_INT64_C(263003643),
    V8_INT64_C(571039860),                V8_INT64_C(1079398689),
    V8_INT64_C(1145196402),               V8_INT64_C(1184846321),
    V8_INT64_C(1758281648),               V8_INT64_C(1859991374),
    V8_INT64_C(1960251588),               V8_INT64_C(2042443199),
    V8_INT64_C(296220586027987448),       V8_INT64_C(1015494173071134726),
    V8_INT64_C(1151237951914455318),      V8_INT64_C(1331941174616854174),
    V8_INT64_C(2022020418667972654),      V8_INT64_C(2450251424374977035),
    V8_INT64_C(3668393562685561486),      V8_INT64_C(4858229301215502171),
    V8_INT64_C(4919426235170669383),      V8_INT64_C(5034286595330341762),
    V8_INT64_C(5055797915536941182),      V8_INT64_C(6072389716149252074),
    V8_INT64_C(6185309910199801210),      V8_INT64_C(6297328311011094138),
    V8_INT64_C(6932372858072165827),      V8_INT64_C(8483640924987737210),
    V8_INT64_C(8663764179455849203),      V8_INT64_C(8877197042645298254),
    V8_INT64_C(8901543506779157333),      V8_INT64_C(9223372036854775807)};


static const uint32_t kUint32Values[] = {
    0x00000000, 0x00000001, 0xffffffff, 0x1b09788b, 0x04c5fce8, 0xcc0de5bf,
    0x273a798e, 0x187937a3, 0xece3af83, 0x5495a16b, 0x0b668ecc, 0x11223344,
    0x0000009e, 0x00000043, 0x0000af73, 0x0000116b, 0x00658ecc, 0x002b3b4c,
    0x88776655, 0x70000000, 0x07200000, 0x7fffffff, 0x56123761, 0x7fffff00,
    0x761c4761, 0x80000000, 0x88888888, 0xa0000000, 0xdddddddd, 0xe0000000,
    0xeeeeeeee, 0xfffffffd, 0xf0000000, 0x007fffff, 0x003fffff, 0x001fffff,
    0x000fffff, 0x0007ffff, 0x0003ffff, 0x0001ffff, 0x0000ffff, 0x00007fff,
    0x00003fff, 0x00001fff, 0x00000fff, 0x000007ff, 0x000003ff, 0x000001ff};

}  // namespace


// -----------------------------------------------------------------------------
// Unary operators


namespace {

struct UnaryOperator {
  Operator* (MachineOperatorBuilder::*constructor)();
  const char* constructor_name;
};


std::ostream& operator<<(std::ostream& os, const UnaryOperator& unop) {
  return os << unop.constructor_name;
}


static const UnaryOperator kUnaryOperators[] = {
    {&MachineOperatorBuilder::ChangeInt32ToFloat64, "ChangeInt32ToFloat64"},
    {&MachineOperatorBuilder::ChangeUint32ToFloat64, "ChangeUint32ToFloat64"},
    {&MachineOperatorBuilder::ChangeFloat64ToInt32, "ChangeFloat64ToInt32"},
    {&MachineOperatorBuilder::ChangeFloat64ToUint32, "ChangeFloat64ToUint32"},
    {&MachineOperatorBuilder::ChangeInt32ToInt64, "ChangeInt32ToInt64"},
    {&MachineOperatorBuilder::ChangeUint32ToUint64, "ChangeUint32ToUint64"},
    {&MachineOperatorBuilder::TruncateFloat64ToInt32, "TruncateFloat64ToInt32"},
    {&MachineOperatorBuilder::TruncateInt64ToInt32, "TruncateInt64ToInt32"}};

}  // namespace


typedef MachineOperatorReducerTestWithParam<UnaryOperator>
    MachineUnaryOperatorReducerTest;


TEST_P(MachineUnaryOperatorReducerTest, Parameter) {
  const UnaryOperator unop = GetParam();
  Reduction reduction =
      Reduce(graph()->NewNode((machine()->*unop.constructor)(), Parameter(0)));
  EXPECT_FALSE(reduction.Changed());
}


INSTANTIATE_TEST_CASE_P(MachineOperatorReducerTest,
                        MachineUnaryOperatorReducerTest,
                        ::testing::ValuesIn(kUnaryOperators));


// -----------------------------------------------------------------------------
// ChangeFloat64ToInt32


TEST_F(MachineOperatorReducerTest,
       ChangeFloat64ToInt32WithChangeInt32ToFloat64) {
  Node* value = Parameter(0);
  Reduction reduction = Reduce(graph()->NewNode(
      machine()->ChangeFloat64ToInt32(),
      graph()->NewNode(machine()->ChangeInt32ToFloat64(), value)));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_EQ(value, reduction.replacement());
}


TEST_F(MachineOperatorReducerTest, ChangeFloat64ToInt32WithConstant) {
  TRACED_FOREACH(int32_t, x, kInt32Values) {
    Reduction reduction = Reduce(graph()->NewNode(
        machine()->ChangeFloat64ToInt32(), Float64Constant(FastI2D(x))));
    ASSERT_TRUE(reduction.Changed());
    EXPECT_THAT(reduction.replacement(), IsInt32Constant(x));
  }
}


// -----------------------------------------------------------------------------
// ChangeFloat64ToUint32


TEST_F(MachineOperatorReducerTest,
       ChangeFloat64ToUint32WithChangeUint32ToFloat64) {
  Node* value = Parameter(0);
  Reduction reduction = Reduce(graph()->NewNode(
      machine()->ChangeFloat64ToUint32(),
      graph()->NewNode(machine()->ChangeUint32ToFloat64(), value)));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_EQ(value, reduction.replacement());
}


TEST_F(MachineOperatorReducerTest, ChangeFloat64ToUint32WithConstant) {
  TRACED_FOREACH(uint32_t, x, kUint32Values) {
    Reduction reduction = Reduce(graph()->NewNode(
        machine()->ChangeFloat64ToUint32(), Float64Constant(FastUI2D(x))));
    ASSERT_TRUE(reduction.Changed());
    EXPECT_THAT(reduction.replacement(), IsInt32Constant(BitCast<int32_t>(x)));
  }
}


// -----------------------------------------------------------------------------
// ChangeInt32ToFloat64


TEST_F(MachineOperatorReducerTest, ChangeInt32ToFloat64WithConstant) {
  TRACED_FOREACH(int32_t, x, kInt32Values) {
    Reduction reduction = Reduce(
        graph()->NewNode(machine()->ChangeInt32ToFloat64(), Int32Constant(x)));
    ASSERT_TRUE(reduction.Changed());
    EXPECT_THAT(reduction.replacement(), IsFloat64Constant(FastI2D(x)));
  }
}


// -----------------------------------------------------------------------------
// ChangeInt32ToInt64


TEST_F(MachineOperatorReducerTest, ChangeInt32ToInt64WithConstant) {
  TRACED_FOREACH(int32_t, x, kInt32Values) {
    Reduction reduction = Reduce(
        graph()->NewNode(machine()->ChangeInt32ToInt64(), Int32Constant(x)));
    ASSERT_TRUE(reduction.Changed());
    EXPECT_THAT(reduction.replacement(), IsInt64Constant(x));
  }
}


// -----------------------------------------------------------------------------
// ChangeUint32ToFloat64


TEST_F(MachineOperatorReducerTest, ChangeUint32ToFloat64WithConstant) {
  TRACED_FOREACH(uint32_t, x, kUint32Values) {
    Reduction reduction =
        Reduce(graph()->NewNode(machine()->ChangeUint32ToFloat64(),
                                Int32Constant(BitCast<int32_t>(x))));
    ASSERT_TRUE(reduction.Changed());
    EXPECT_THAT(reduction.replacement(), IsFloat64Constant(FastUI2D(x)));
  }
}


// -----------------------------------------------------------------------------
// ChangeUint32ToUint64


TEST_F(MachineOperatorReducerTest, ChangeUint32ToUint64WithConstant) {
  TRACED_FOREACH(uint32_t, x, kUint32Values) {
    Reduction reduction = Reduce(graph()->NewNode(
        machine()->ChangeUint32ToUint64(), Int32Constant(BitCast<int32_t>(x))));
    ASSERT_TRUE(reduction.Changed());
    EXPECT_THAT(reduction.replacement(),
                IsInt64Constant(BitCast<int64_t>(static_cast<uint64_t>(x))));
  }
}


// -----------------------------------------------------------------------------
// TruncateFloat64ToInt32


TEST_F(MachineOperatorReducerTest,
       TruncateFloat64ToInt32WithChangeInt32ToFloat64) {
  Node* value = Parameter(0);
  Reduction reduction = Reduce(graph()->NewNode(
      machine()->TruncateFloat64ToInt32(),
      graph()->NewNode(machine()->ChangeInt32ToFloat64(), value)));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_EQ(value, reduction.replacement());
}


TEST_F(MachineOperatorReducerTest, TruncateFloat64ToInt32WithConstant) {
  TRACED_FOREACH(double, x, kFloat64Values) {
    Reduction reduction = Reduce(graph()->NewNode(
        machine()->TruncateFloat64ToInt32(), Float64Constant(x)));
    ASSERT_TRUE(reduction.Changed());
    EXPECT_THAT(reduction.replacement(), IsInt32Constant(DoubleToInt32(x)));
  }
}


// -----------------------------------------------------------------------------
// TruncateInt64ToInt32


TEST_F(MachineOperatorReducerTest, TruncateInt64ToInt32WithChangeInt32ToInt64) {
  Node* value = Parameter(0);
  Reduction reduction = Reduce(graph()->NewNode(
      machine()->TruncateInt64ToInt32(),
      graph()->NewNode(machine()->ChangeInt32ToInt64(), value)));
  ASSERT_TRUE(reduction.Changed());
  EXPECT_EQ(value, reduction.replacement());
}


TEST_F(MachineOperatorReducerTest, TruncateInt64ToInt32WithConstant) {
  TRACED_FOREACH(int64_t, x, kInt64Values) {
    Reduction reduction = Reduce(
        graph()->NewNode(machine()->TruncateInt64ToInt32(), Int64Constant(x)));
    ASSERT_TRUE(reduction.Changed());
    EXPECT_THAT(reduction.replacement(),
                IsInt32Constant(BitCast<int32_t>(
                    static_cast<uint32_t>(BitCast<uint64_t>(x)))));
  }
}


// -----------------------------------------------------------------------------
// Word32Ror


TEST_F(MachineOperatorReducerTest, ReduceToWord32RorWithParameters) {
  Node* value = Parameter(0);
  Node* shift = Parameter(1);
  Node* shl = graph()->NewNode(machine()->Word32Shl(), value, shift);
  Node* shr = graph()->NewNode(
      machine()->Word32Shr(), value,
      graph()->NewNode(machine()->Int32Sub(), Int32Constant(32), shift));

  // (x << y) | (x >> (32 - y)) => x ror y
  Node* node1 = graph()->NewNode(machine()->Word32Or(), shl, shr);
  Reduction reduction1 = Reduce(node1);
  EXPECT_TRUE(reduction1.Changed());
  EXPECT_EQ(reduction1.replacement(), node1);
  EXPECT_THAT(reduction1.replacement(), IsWord32Ror(value, shift));

  // (x >> (32 - y)) | (x << y) => x ror y
  Node* node2 = graph()->NewNode(machine()->Word32Or(), shr, shl);
  Reduction reduction2 = Reduce(node2);
  EXPECT_TRUE(reduction2.Changed());
  EXPECT_EQ(reduction2.replacement(), node2);
  EXPECT_THAT(reduction2.replacement(), IsWord32Ror(value, shift));
}


TEST_F(MachineOperatorReducerTest, ReduceToWord32RorWithConstant) {
  Node* value = Parameter(0);
  TRACED_FORRANGE(int32_t, k, 0, 31) {
    Node* shl =
        graph()->NewNode(machine()->Word32Shl(), value, Int32Constant(k));
    Node* shr =
        graph()->NewNode(machine()->Word32Shr(), value, Int32Constant(32 - k));

    // (x << K) | (x >> ((32 - K) - y)) => x ror K
    Node* node1 = graph()->NewNode(machine()->Word32Or(), shl, shr);
    Reduction reduction1 = Reduce(node1);
    EXPECT_TRUE(reduction1.Changed());
    EXPECT_EQ(reduction1.replacement(), node1);
    EXPECT_THAT(reduction1.replacement(),
                IsWord32Ror(value, IsInt32Constant(k)));

    // (x >> (32 - K)) | (x << K) => x ror K
    Node* node2 = graph()->NewNode(machine()->Word32Or(), shr, shl);
    Reduction reduction2 = Reduce(node2);
    EXPECT_TRUE(reduction2.Changed());
    EXPECT_EQ(reduction2.replacement(), node2);
    EXPECT_THAT(reduction2.replacement(),
                IsWord32Ror(value, IsInt32Constant(k)));
  }
}


TEST_F(MachineOperatorReducerTest, Word32RorWithZeroShift) {
  Node* value = Parameter(0);
  Node* node =
      graph()->NewNode(machine()->Word32Ror(), value, Int32Constant(0));
  Reduction reduction = Reduce(node);
  EXPECT_TRUE(reduction.Changed());
  EXPECT_EQ(reduction.replacement(), value);
}


TEST_F(MachineOperatorReducerTest, Word32RorWithConstants) {
  TRACED_FOREACH(int32_t, x, kUint32Values) {
    TRACED_FORRANGE(int32_t, y, 0, 31) {
      Node* node = graph()->NewNode(machine()->Word32Ror(), Int32Constant(x),
                                    Int32Constant(y));
      Reduction reduction = Reduce(node);
      EXPECT_TRUE(reduction.Changed());
      EXPECT_THAT(reduction.replacement(),
                  IsInt32Constant(base::bits::RotateRight32(x, y)));
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
