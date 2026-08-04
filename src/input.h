#pragma once

// Compensacao de recuo e hotkeys.
//
// ATENCAO: esta fase move o codigo de lugar sem mudar a logica. Os defeitos
// conhecidos continuam presentes de proposito, para que o commit da migracao
// de render nao se misture com mudanca de comportamento:
//
//   - mouse_event em modo absoluto, quando o correto e relativo
//   - 'rate' aplicado por iteracao do laco, nao por unidade de tempo
//   - GetKeyState onde deveria ser GetAsyncKeyState
//
// A fase 2 reescreve tudo isso.
namespace input {

void Tick();

bool IsActive();
int  Rate();

}  // namespace input
