#pragma once

#include <QtGlobal>

class QWidget;
class TitleBar;

namespace Win {

void prepareFrameless(QWidget *window);
bool handleNativeEvent(QWidget *window, TitleBar *titleBar, void *message, qintptr *result);
void installVirtualFileSupport();

}
