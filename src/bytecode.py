from enum import IntEnum, auto
from dataclasses import dataclass, field
from typing import List, Any, Optional

class OpCode(IntEnum):
    OP_CONST = auto()         # Charge une constante 
    OP_LOAD = auto()          # Charge une variable locale 
    OP_STORE = auto()         # Stocke le sommet de pile dans une variable 
    OP_ADD = auto()           # Additionne les deux valeurs au sommet
    OP_SUB = auto()           # Soustrait (a - b)
    OP_MUL = auto()           # Multiplie (a * b)
    OP_DIV = auto()           # Division entière (a // b)
    OP_LT = auto()            # Comparaison stricte inférieure (a < b)
    OP_EQ = auto()            # Égalité (a == b)
    OP_JUMP = auto()          # Saut inconditionnel vers un index d'instruction
    OP_JUMP_IF_FALSE = auto() # Saut si faux (0)
    OP_CALL = auto()          # Appel de fonction
    OP_RET = auto()           # Retourne la valeur du sommet

@dataclass
class Instruction:
    op: OpCode
    arg: int = 0

@dataclass
class BytecodeFunction:
    name: str
    num_args: int
    num_locals: int
    constants: List[int] = field(default_factory=list)
    code: List[Instruction] = field(default_factory=list)
    call_count: int = 0
    native_fn: Optional[Any] = None

    def add_constant(self, val: int) -> int:
        self.constants.append(val)
        return len(self.constants) - 1

    def emit(self, op: OpCode, arg: int = 0) -> int:
        self.code.append(Instruction(op, arg))
        return len(self.code) - 1

    def dump(self):
        print(f"=== Bytecode pour la fonction: {self.name} ===")
        print(f"Arguments: {self.num_args}, Variables locales: {self.num_locals}")
        print(f"Constantes ({len(self.constants)}) : {self.constants}")
        print(f"Instructions ({len(self.code)}) :")
        for i, inst in enumerate(self.code):
            print(f"  {i:04d}: {inst.op.name:<18} {inst.arg}")
        print("=" * 40 + "\n")