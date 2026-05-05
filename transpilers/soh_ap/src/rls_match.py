from typing import Any
# from enum import IntEnum, auto

# class EnemyDistances(IntEnum):
#     ED_CLOSE = auto()
#     ED_SHORT_JUMPSLASH = auto()
#     ED_MASTER_SWORD_JUMPSLASH = auto()
#     ED_LONG_JUMPSLASH = auto()
#     ED_BOMB_THROW = auto()
#     ED_BOOMERANG = auto()
#     ED_HOOKSHOT = auto()
#     ED_LONGSHOT = auto()
#     ED_FAR = auto()

# class RandomizerGet(IntEnum):
#     RG_BOOMERANG = auto()
#     RG_HOOKSHOT = auto()
#     RG_LONGSHOT = auto()

# # Unpack members into current namespace
# RG_BOOMERANG, RG_HOOKSHOT, RG_LONGSHOT = RandomizerGet
# ED_CLOSE, ED_SHORT_JUMPSLASH, ED_MASTER_SWORD_JUMPSLASH, ED_LONG_JUMPSLASH, ED_BOMB_THROW, ED_BOOMERANG, ED_HOOKSHOT, ED_LONGSHOT, ED_FAR = EnemyDistances

# not sure how this active variable is used or changed
active = False

def rls_match(compare, condition, body, fallthrough: bool, *args) -> Any:
    if len(args) == 0:
        if active or condition(compare): return body()
        # Choose default for type to return
        if type(body()) == bool: return False
        if type(body()) == int: return 0
    else:
        if active or condition(compare):
            if fallthrough:
                if bool(body()): return body()
                return rls_match(compare, *args)
            return body()
        return rls_match(compare, *args)

# distance = EnemyDistances.ED_FAR

# def can_use(x):
#     return True

# print(rls_match(distance, (lambda distance: distance == ED_CLOSE or distance == ED_SHORT_JUMPSLASH or distance == ED_MASTER_SWORD_JUMPSLASH or distance == ED_LONG_JUMPSLASH or distance == ED_BOMB_THROW or distance == ED_BOOMERANG), (lambda: can_use(RG_BOOMERANG)), True, (lambda distance: distance == ED_HOOKSHOT), (lambda: can_use(RG_HOOKSHOT)), True, (lambda distance: distance == ED_LONGSHOT), (lambda: can_use(RG_LONGSHOT)), False))