// compute a small expression step by step

// 1) define some variables
let a = 10;
let b = 3;
let c = (a - b) * 2;   // (10 - 3) * 2 = 14

// 2) use them in another calculation
let d = c / b;         // 14 / 3 = 4 (integer division)

// 3) return the final result
yield(a + b + c + d);    // 10 + 3 + 14 + 4 = 31