#
# Copyright 2016 by Shaheed Haque (srhaque@theiet.org)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301  USA.
#
"""
SIP binding customisation for PyKF5.KWidgetAddons. This modules describes:

    * The SIP file generator rules.
"""

import os, sys

import rules_engine
sys.path.append(os.path.dirname(os.path.dirname(rules_engine.__file__)))
import Qt5Ruleset

from copy import deepcopy

from clang.cindex import AccessSpecifier, CursorKind

def _typedef_discard(container, typedef, sip, matcher):
    sip["name"] = ""

def _discard_QSharedData(container, sip, matcher):
    sip["base_specifiers"] = sip["base_specifiers"].replace(", QSharedData", "")

def _discard_KConfigBase(container, sip, matcher):
    sip["base_specifiers"] = sip["base_specifiers"].replace("KConfigBase", "").strip()

def _container_discard_base(container, sip, matcher):
    sip["base_specifiers"] = ""

def set_skeleton_item_base(container, sip, matcher):
    if not sip["base_specifiers"] or sip["base_specifiers"].endswith(">"):
        sip["base_specifiers"] = "KConfigSkeletonItem"

def set_skeleton_item_base_gui(container, sip, matcher):
    sip["base_specifiers"] = "KConfigSkeletonItem"

def mark_abstract(container, sip, matcher):
    sip["annotations"].add("Abstract")

def mark_and_discard(container, sip, matcher):
    sip["annotations"].add("Abstract")
    sip["base_specifiers"] = sip["base_specifiers"].replace(", QSharedData", "")


def local_container_rules():
    return [
        [".*", "KConfigCompilerSignallingItem.*", ".*", ".*", ".*", rules_engine.container_discard],

        [".*", "KConfigBackend", ".*", ".*", ".*", mark_and_discard],
        [".*", "KConfigBase", ".*", ".*", ".*", mark_abstract],

        [".*KCoreConfigSkeleton.*", ".*Item.*", ".*", ".*", ".*", set_skeleton_item_base],

        ["KConfigSkeleton", "ItemColor", ".*", ".*", ".*", set_skeleton_item_base_gui],
        ["KConfigSkeleton", "ItemFont", ".*", ".*", ".*", set_skeleton_item_base_gui],

        [".*", "KSharedConfig", ".*", ".*", ".*", _discard_QSharedData],
    ]

def noop(container, function, sip, matcher):
    pass

def local_function_rules():
    return [
        ["KConfigBase", "group", ".*", ".*", ".*const char.*", rules_engine.function_discard],
        ["KConfigBase", "group", ".*", ".*", ".*QByteArray.*", rules_engine.function_discard],
        ["KConfigBase", "group", ".*", "const KConfigGroup", ".*", rules_engine.function_discard],
        ["KConfigBase", "groupImpl", ".*", ".*", ".*", rules_engine.function_discard],
        ["KConfig", "groupImpl", ".*", ".*", ".*", rules_engine.function_discard],

        ["KConfigBackend", "registerMappings", ".*", ".*", ".*", rules_engine.function_discard],
        ["KConfigBackend", "create", ".*", ".*", ".*", rules_engine.function_discard],

        ["KSharedConfig", "openConfig", ".*", ".*", ".*", rules_engine.function_discard],

        [".*", ".*", ".*", ".*", ".*KEntryMap", rules_engine.function_discard],

        ["KConfigGroup", "KConfigGroup", ".*", ".*", ".*KConfigBase.*", rules_engine.function_discard],
        ["KConfigGroup", "config", ".*", "const KConfig.*", ".*", rules_engine.function_discard],
        ["KConfigGroup", "groupImpl", ".*", "const KConfig.*", ".*", rules_engine.function_discard],

        ["KDesktopFile", "actionGroup", ".*", "const KConfigGroup.*", ".*", rules_engine.function_discard],

        ["KDesktopFile", ".*", ".*", "KConfigGroup", ".*", rules_engine.function_discard],
        ["KConfigGroup", ".*", ".*", "KConfigGroup", ".*", rules_engine.function_discard],
        ["KConfigBase", ".*", ".*", "KConfigGroup", ".*", rules_engine.function_discard],

        ["KCoreConfigSkeleton", "config", ".*", "const KConfig.*", ".*", rules_engine.function_discard],
        ["KCoreConfigSkeleton", "sharedConfig", ".*", ".*", ".*", rules_engine.function_discard],

        ["KCoreConfigSkeleton::ItemIntList", "ItemIntList", ".*", ".*", ".*", noop],
        ["KCoreConfigSkeleton::ItemUrlList", "ItemUrlList", ".*", ".*", ".*", noop],
        ["KCoreConfigSkeleton::ItemEnum", "ItemEnum", ".*", ".*", ".*", noop],
    ]

def local_typedef_rules():
    return [
        [".*", ".*", ".*", "typedef QExplicitlySharedDataPointer<KSharedConfig> KSharedConfigPtr", _typedef_discard],
        ["KConfigSkeletonItem", "DictIterator", ".*", ".*", _typedef_discard],
    ]


def _kcoreconfigskeleton_item_xxx(function, sip, entry):
    sip["decl2"] = deepcopy(sip["decl"])
    sip["fn_result2"] = ""
    sip["code"] = """
        %MethodCode
            Py_BEGIN_ALLOW_THREADS
            //    sipCpp = new sipKCoreConfigSkeleton_Item{} (PyItem{} (*a0, *a1, a2, a3));
            sipCpp = new sipKCoreConfigSkeleton_Item{} (*a0, *a1, a2, a3);
            Py_END_ALLOW_THREADS
        %End
        """.replace("{}", entry["ctx"])
    sip["decl"][2] = sip["decl"][2].replace("&", "")


def _kcoreconfigskeleton_item_enum(function, sip, entry):
    sip["decl2"] = deepcopy(sip["decl"])
    sip["fn_result2"] = ""
    sip["code"] = """
        %MethodCode
            Py_BEGIN_ALLOW_THREADS
            //    sipCpp = new sipKCoreConfigSkeleton_ItemEnum (PyItemEnum (*a0, *a1, a2, *a3, a4));
            sipCpp = new sipKCoreConfigSkeleton_ItemEnum (*a0, *a1, a2, *a3, a4);
            Py_END_ALLOW_THREADS
        %End
        """.replace("{}", entry["ctx"])
    sip["decl"][2] = sip["decl"][2].replace("&", "")


def _kcoreconfigskeleton_add_item_xxx(function, sip, entry):
    sip["code"] = """
        %MethodCode
            Py_BEGIN_ALLOW_THREADS
            sipRes = new PyItem{} (sipCpp->currentGroup(), a3->isNull() ? *a0 : *a3, a1, a2);
            sipCpp->addItem(sipRes, *a0);
            Py_END_ALLOW_THREADS
        %End
        """.format(entry["ctx"])

def _kcoreconfigskeleton_item_add_py_subclass(filename, sip, entry):
    result = """
%ModuleHeaderCode
#include <kcoreconfigskeleton.h>
"""
    for ctx in ({"Type": "Bool", "cpptype": "bool", "defaultValue": 1},
            {"Type": "Int", "cpptype": "qint32", "defaultValue": 1},
            {"Type": "UInt", "cpptype": "quint32", "defaultValue": 1},
            {"Type": "LongLong", "cpptype": "qint64", "defaultValue": 1},
            {"Type": "ULongLong", "cpptype": "quint64", "defaultValue": 1},
            {"Type": "Double", "cpptype": "double", "defaultValue": 1},
        ):
        result += """
class PyItem{Type} : public KCoreConfigSkeleton::Item{Type}
{{
public:
    PyItem{Type} (const QString &group, const QString &key, {cpptype}& val, {cpptype} defaultValue = {defaultValue}) :
        KCoreConfigSkeleton::Item{Type} (group, key, this->value, defaultValue),
        value(val)
    {{
    }}

private:
    {cpptype} value;
}};
""".format(**ctx)

    result += """
class PyItemEnum : public KCoreConfigSkeleton::ItemEnum
{
public:
    PyItemEnum (const QString& group, const QString& key, int& val, const QList<KCoreConfigSkeleton::ItemEnum::Choice>& choices, int defaultValue = 0) :
        KCoreConfigSkeleton::ItemEnum(group, key, this->value, choices, defaultValue),
        value(val)
    {
    };

private:
    int value;
};
%End\n
"""

    sip["code"] = result


class RuleSet(Qt5Ruleset.RuleSet):
    """
    SIP file generator rules. This is a set of (short, non-public) functions
    and regular expression-based matching rules.
    """
    def __init__(self, includes):
        Qt5Ruleset.RuleSet.__init__(self, includes)
        self._fn_db = rules_engine.FunctionRuleDb(lambda: local_function_rules() + Qt5Ruleset.function_rules())
        self._container_db = rules_engine.ContainerRuleDb(lambda: local_container_rules() + Qt5Ruleset.container_rules())
        self._typedef_db = rules_engine.TypedefRuleDb(lambda: local_typedef_rules() + Qt5Ruleset.typedef_rules())
        self._methodcode = rules_engine.MethodCodeDb({
            "KCoreConfigSkeleton::ItemBool": #"kdecore/kcoreconfigskeleton.h"
            {
                "ItemBool":
                {
                    "code": _kcoreconfigskeleton_item_xxx,
                    "ctx": "Bool",
                },
            },
            "KCoreConfigSkeleton::ItemInt": #"kdecore/kcoreconfigskeleton.h"
            {
                "ItemInt":
                {
                    "code": _kcoreconfigskeleton_item_xxx,
                    "ctx": "Int",
                },
            },
            "KCoreConfigSkeleton::ItemLongLong": #"kdecore/kcoreconfigskeleton.h"
            {
                "ItemLongLong":
                {
                    "code": _kcoreconfigskeleton_item_xxx,
                    "ctx": "LongLong",
                },
            },
            "KCoreConfigSkeleton::ItemEnum": #"kdecore/kcoreconfigskeleton.h"
            {
                "ItemEnum":
                {
                    "code": _kcoreconfigskeleton_item_enum,
                    "ctx": "Enum",
                },
            },
            "KCoreConfigSkeleton::ItemUInt": #"kdecore/kcoreconfigskeleton.h"
            {
                "ItemUInt":
                {
                    "code": _kcoreconfigskeleton_item_xxx,
                    "ctx": "UInt",
                },
            },
            "KCoreConfigSkeleton::ItemULongLong": #"kdecore/kcoreconfigskeleton.h"
            {
                "ItemULongLong":
                {
                    "code": _kcoreconfigskeleton_item_xxx,
                    "ctx": "ULongLong",
                },
            },
            "KCoreConfigSkeleton::ItemDouble": #"kdecore/kcoreconfigskeleton.h"
            {
                "ItemDouble":
                {
                    "code": _kcoreconfigskeleton_item_xxx,
                    "ctx": "Double",
                },
            },
            "KCoreConfigSkeleton": #"kdecore/kcoreconfigskeleton.h"
            {
                "addItemBool":
                {
                    "code": _kcoreconfigskeleton_add_item_xxx,
                    "ctx": "Bool",
                },
                "addItemInt":
                {
                    "code": _kcoreconfigskeleton_add_item_xxx,
                    "ctx": "Int",
                },
                "addItemUInt":
                {
                    "code": _kcoreconfigskeleton_add_item_xxx,
                    "ctx": "UInt",
                },
                "addItemLongLong":
                {
                    "code": _kcoreconfigskeleton_add_item_xxx,
                    "ctx": "LongLong",
                },
                "addItemInt64":
                {
                    "code": _kcoreconfigskeleton_add_item_xxx,
                    "ctx": "LongLong",
                },
                "addItemULongLong":
                {
                    "code": _kcoreconfigskeleton_add_item_xxx,
                    "ctx": "ULongLong",
                },
                "addItemUInt64":
                {
                    "code": _kcoreconfigskeleton_add_item_xxx,
                    "ctx": "ULongLong",
                },
                "addItemDouble":
                {
                    "code": _kcoreconfigskeleton_add_item_xxx,
                    "ctx": "Double",
                },
            },
        })

        self._modulecode = rules_engine.ModuleCodeDb({
            "KConfigSkeletonItem":
            {
                "code": _kcoreconfigskeleton_item_add_py_subclass,
            },
            })
