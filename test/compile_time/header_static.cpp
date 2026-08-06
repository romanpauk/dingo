#include <dingo/static_container.h>

int main() {
  using source = dingo::bindings<>;
  dingo::static_container<source> container;
  (void)container;
  return 0;
}
