from bytecode import BytecodeFunction, OpCode
from vm import VM

def build_sum_function() -> BytecodeFunction:
   
    fn = BytecodeFunction(name="sum_to", num_args=1, num_locals=2)

    # Constantes
    c0 = fn.add_constant(0)
    c1 = fn.add_constant(1)

    # 0000: acc = 0
    fn.emit(OpCode.OP_CONST, c0)
    fn.emit(OpCode.OP_STORE, 1)

    # --- DEBUT BOUCLE (ip = 2) ---
    loop_start = len(fn.code)
    # 0002: 0 < n  (n > 0)
    fn.emit(OpCode.OP_CONST, c0)
    fn.emit(OpCode.OP_LOAD, 0)
    fn.emit(OpCode.OP_LT)

    # 0005: Saut si faux vers la sortie
    jump_exit = fn.emit(OpCode.OP_JUMP_IF_FALSE, 0)

    # 0006: acc = acc + n
    fn.emit(OpCode.OP_LOAD, 1)
    fn.emit(OpCode.OP_LOAD, 0)
    fn.emit(OpCode.OP_ADD)
    fn.emit(OpCode.OP_STORE, 1)

    # 0010: n = n - 1
    fn.emit(OpCode.OP_LOAD, 0)
    fn.emit(OpCode.OP_CONST, c1)
    fn.emit(OpCode.OP_SUB)
    fn.emit(OpCode.OP_STORE, 0)

    # 0014: boucle
    fn.emit(OpCode.OP_JUMP, loop_start)

    # --- RETOUCHE (Backpatch) DE L'ADRESSE DE SORTIE ---
    fn.code[jump_exit].arg = len(fn.code)

    # 0015: return acc
    fn.emit(OpCode.OP_LOAD, 1)
    fn.emit(OpCode.OP_RET)

    return fn

if __name__ == "__main__":
    fn = build_sum_function()
    fn.dump()

    vm = VM()
    n = 10
    resultat = vm.run(fn, [n])
    print(f">> Résultat de sum_to({n}) sur la VM : {resultat}")