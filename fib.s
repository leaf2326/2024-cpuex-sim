  movi x31, 10
  mov x10, x31
  call fib_1
  mov x10, x10
  ebreak
  halt
fib_1:
  sw x1, -4(x2)
  sw x8, -8(x2)
  mov x8, x2
  sw x29, -12(x2)
  sw x30, -16(x2)
  sw x31, -20(x2)
  addi x2, x2, -32
  mov x31, x10
  movi x30, 0
  bne x31, x30, Lelse12
Lthen12:
  movi x10, 0
  jal x0, Lend12
Lelse12:
  movi x30, 1
  bne x31, x30, Lelse13
Lthen13:
  movi x10, 1
  jal x0, Lend13
Lelse13:
  movi x30, 1
  sub x30, x31, x30
  mov x10, x30
  call fib_1
  mov x30, x10
  movi x29, 2
  sub x29, x31, x29
  mov x10, x29
  call fib_1
  mov x29, x10
  add x10, x30, x29
Lend13:
Lend12:
  mov x2, x8
  lw x1, -4(x2)
  lw x8, -8(x2)
  lw x29, -12(x2)
  lw x30, -16(x2)
  lw x31, -20(x2)
  ret
