"""Content of the key kinds section.

A key distinguishes bindings of one interface. Keys of one domain, the
interface and the key type, are compared: a tag key by its type, and a value
key by its value, whether the value is fixed when the program is compiled or
supplied at runtime, so a fixed key and a runtime key of equal value name the
same bindings. Every key kind works in every container.
"""

from spec.schema import BindingTime, KeyKind

key_kinds: tuple[KeyKind, ...] = (
    KeyKind(id="none", description="No key: the binding is named by its interface."),
    KeyKind(
        id="tag",
        description="A type that names the binding, without a value.",
    ),
    KeyKind(
        id="fixed",
        description="A value of a key type fixed when the program is compiled.",
        value_time=BindingTime.COMPILE,
    ),
    KeyKind(
        id="runtime",
        description="A value of a key type supplied at runtime.",
        value_time=BindingTime.RUNTIME,
    ),
)
