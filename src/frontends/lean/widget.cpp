/*
Copyright (c) E.W.Ayers. All rights reserved.
Released under Apache 2.0 license as described in the file LICENSE.

Author: E.W.Ayers
*/
#include <map>
#include <vector>
#include <string>
#include <atomic>
#include "library/vm/vm.h"
#include "library/vm/vm_option.h"
#include "library/vm/vm_string.h"
#include "library/vm/vm_list.h"
#include "library/vm/vm_task.h"
#include "util/list.h"
#include "frontends/lean/widget.h"
#include "frontends/lean/json.h"
#include "util/optional.h"
#include "util/pair.h"

namespace lean {

enum mouse_capture_state {
    outside = 0,
    inside_immediate = 1,
    inside_child = 2
};

enum component_idx {
    pure = 0,
    filter_map_action = 1,
    map_props = 2,
    with_should_update = 3,
    with_state = 4,
    with_task = 5,
    with_mouse_capture = 6
};
enum html_idx {
    element = 7,
    of_string = 8,
    of_component = 9
};
enum attr_idx {
    val = 10,
    mouse_event = 11,
    style = 12,
    tooltip = 13,
    text_change_event = 14
};

std::atomic_uint g_fresh_handler_id;
std::atomic_uint g_fresh_component_instance_id;

optional<std::string> vdom_element::key() {
    if (m_attrs.find("key") != m_attrs.end()) {
        // there is an entry with key "key"
        std::string k = m_attrs["key"];
        return optional<std::string>(k);
    }
    return optional<std::string>();
}
void vdom_element::reconcile(vdom const & old_vdom) {
    vdom_element * o = dynamic_cast<vdom_element*>(old_vdom.raw());
    if (o && o->m_tag == m_tag) {
        std::vector<vdom> ocs = o->m_children;
        reconcile_children(m_children, ocs);
        if (m_tooltip && o->m_tooltip) {
            (*m_tooltip).reconcile(*(o->m_tooltip));
        }
    }
}

json route_to_json(list<unsigned> const & route) {
    json jr = json::array();
    for (auto const & i : route) {
        jr.push_back(i);
    }
    return jr;
}

json vdom_element::to_json(list<unsigned> const & route) {
    json entry;
    entry["t"] = m_tag;
    entry["a"] = m_attrs;

    for (auto const & x : m_events) {
        entry["e"][x.first]["r"] = route_to_json(route);
        entry["e"][x.first]["h"] = json(x.second);
    }
    entry["c"] = json::array();
    for (vdom & v : m_children) {
        entry["c"].push_back(v.to_json(route));
    }
    if (m_tooltip) {
        entry["tt"] = (*m_tooltip).to_json(route);
    }
    return entry;
}

struct filter_map_action_hook : public component_hook {
    ts_vm_obj const m_map;
    ts_vm_obj m_props;
    virtual void initialize(vm_obj const & props) override {
        m_props = props;
    }
    virtual bool reconcile(vm_obj const & props, component_hook const & prev) override {
        m_props = props;
        return true;
    }
    virtual optional<vm_obj> action(vm_obj const & action) override {
        lean_assert(m_props);
        vm_obj o = invoke(m_map.to_vm_obj(), m_props.to_vm_obj(), action);
        return get_optional(o);
    }
    filter_map_action_hook(ts_vm_obj const map): m_map(map) {}
};

struct map_props_hook: public component_hook {
    ts_vm_obj const m_map;
    virtual vm_obj get_props(vm_obj const & props) override {
        return invoke(m_map.to_vm_obj(), props);
    }
    map_props_hook(ts_vm_obj const map): m_map(map) {}
};

struct with_should_update_hook : public component_hook {
    ts_vm_obj const m_su;
    ts_vm_obj m_props;
    virtual void initialize(vm_obj const & props) override {
        m_props = props;
    }
    virtual bool reconcile(vm_obj const & new_props, component_hook const & previous) override {
        with_should_update_hook const * prev = dynamic_cast<with_should_update_hook const *>(&previous);
        if (!prev) {return true;}
        vm_obj prev_props = (prev->m_props).to_vm_obj();
        if (!prev_props) {return true;}
        m_props = new_props;
        return to_bool(invoke(m_su.to_vm_obj(), prev_props, new_props));
    }
    with_should_update_hook(ts_vm_obj const su): m_su(su) {}
};

struct stateful_hook : public component_hook {
    ts_vm_obj const m_init;
    ts_vm_obj const m_update;
    ts_vm_obj m_props;
    optional<ts_vm_obj> m_state;
    void initialize(vm_obj const & props) override {
        vm_obj s = m_state ? mk_vm_none() : mk_vm_some((*m_state).to_vm_obj());
        vm_obj r = invoke(m_init.to_vm_obj(), props, s);
        m_state = r;
        m_props = props;
    }
    bool reconcile(vm_obj const & props, component_hook const & previous) override {
        stateful_hook const * prev = dynamic_cast<stateful_hook const *>(&previous);
        // assume the props have changed
        if (prev) {
            m_state = prev->m_state;
            initialize(props);
        }
        initialize(props);
        return true;
    }
    vm_obj get_props(vm_obj const & props) override {
        if (!m_state) {initialize(props);}
        return mk_vm_pair((*m_state).to_vm_obj(), props);
    }
    optional<vm_obj> action(vm_obj const & action) override {
        lean_assert(m_state);
        lean_assert(m_props);
        vm_obj r = invoke(m_update.to_vm_obj(), m_props.to_vm_obj(), (*m_state).to_vm_obj(), action);
        m_state = cfield(r,0);
        return get_optional(cfield(r,1));
    }
    stateful_hook(vm_obj const & init, vm_obj const & update) : m_init(init), m_update(update) {}
};

struct with_mouse_capture_hook : public component_hook {
    mouse_capture_state m_s;
    vm_obj get_props(vm_obj const & props) override {
        return mk_vm_pair(mk_vm_simple(0), props);
    }
    bool set_state(mouse_capture_state s) {
        if (m_s == s) {return false;}
        m_s = s;
        return true;
    }
    with_mouse_capture_hook() {}
};
struct with_task_hook : public component_hook {
    ts_vm_obj const m_tb;
    task<ts_vm_obj> m_task;
    virtual bool reconcile(vm_obj const & props, component_hook const & old) override {
        // assume that the props have changed. so we have to just recompute.
        // with_task_hook const * t_old = dynamic_cast<with_task_hook const *>(&old);
        initialize(props);
        return true;
    }
    virtual void initialize(vm_obj const & props) override {
        if (m_task) {return; }
        vm_obj vt = invoke(m_tb.to_vm_obj(), props);
        m_task = to_task(vt);
        taskq().submit(m_task);
        // [todo]: set up a handler for when the task completes.
        // unsigned handler_id = g_fresh_handler_id.fetch_add(1);
        // m_handler = handler_id;
        // auto route = cons(m_id, m_route);
        // pending_tasks().push_back(task_builder<list<unsigned>>([route] {
        //     return route;
        // }).depends_on(t).build());
    }
    vm_obj get_props(vm_obj const & props) override {
        optional<ts_vm_obj> result = peek(m_task);
        return mk_vm_pair(result ? mk_vm_some((*result).to_vm_obj()) : mk_vm_none(), props);
    }
    with_task_hook(vm_obj const & tb): m_tb(tb) {}
    ~with_task_hook() {
        if (m_task) {
            taskq().fail_and_dispose(m_task); //hopefully this doesn't error if it's already disposed.
        }
    }
};

component_instance::component_instance(vm_obj const & component, vm_obj const & props, list<unsigned> const & route):
  m_props(props), m_route(route) {
    m_id = g_fresh_component_instance_id.fetch_add(1);
    m_reconcile_count = 0;
    m_component_hash = hash(component);
    vm_obj c = component;
    while (cidx(c) != component_idx::pure) {
        switch (cidx(c)) {
            case component_idx::pure: break;
            case component_idx::filter_map_action:
                m_hooks.push_back(filter_map_action_hook(cfield(c, 0)));
                c = cfield(c,1);
                break;
            case component_idx::map_props:
                m_hooks.push_back(map_props_hook(cfield(c,0)));
                c = cfield(c,1);
                break;
            case component_idx::with_should_update:
                m_hooks.push_back(with_should_update_hook(cfield(c,0)));
                c = cfield(c,1);
                break;
            case component_idx::with_state:
                m_hooks.push_back(stateful_hook(cfield(c,0), cfield(c,1)));
                c = cfield(c,2);
                break;
            case component_idx::with_task:
                m_hooks.push_back(with_task_hook(cfield(c,0)));
                c = cfield(c,1);
                break;
            case component_idx::with_mouse_capture:
                m_hooks.push_back(with_mouse_capture_hook());
                c = cfield(c,0);
                break;
            default:
                lean_unreachable();
                break;
        }
    }
    m_view = cfield(c,0);
}

void component_instance::render() {
    lean_assert(m_inner_props);
    std::vector<component_instance *> children;
    std::map<unsigned, ts_vm_obj> handlers;
    vm_obj view = invoke(m_view.to_vm_obj(), m_inner_props.to_vm_obj());
    std::vector<vdom> elements = render_html_list(view, children, handlers, cons(m_id, m_route));
    std::vector<vdom> old_elements = m_render;
    reconcile_children(elements, old_elements);
    m_handlers = handlers;
    m_children = children;
    m_render = elements;
    m_has_rendered = true;
}

void component_instance::reconcile(vdom const & old) {
    lean_assert(!m_has_rendered);
    component_instance * ci_old = dynamic_cast<component_instance *>(old.raw());
    // If they contain vm_externals which are not hashable then we assume they are the same component.
    // This is acceptable, but confusing, behaviour for now. It just means that the component won't always
    // update correctly if a non-prop dependency of a component changes.
    // But users of components should be using Props anyway so there is a workaround.
    if (ci_old && ci_old->m_component_hash == m_component_hash) {
        // if the components are the same:
        // note that this doesn't occur if they do the same thing but were made with different calls to component.mk.
        vm_obj p_new = m_props.to_vm_obj();
        vm_obj p_old = ci_old->m_props.to_vm_obj();
        lean_assert(m_hooks.length == ci_old->m_hooks.size());
        bool should_update = p_new != p_old;
        for (unsigned i = 0; i < m_hooks.size(); i++) {
            if (should_update) {
                should_update &= m_hooks[i].reconcile(p_new, ci_old->m_hooks[i]);
            }
            if (!should_update) {
                m_hooks[i] = ci_old->m_hooks[i];
            } else {
                p_new = m_hooks[i].get_props(p_new);
            }
        }

        if (!should_update) {
            // the props are equal and the state didn't change, so we can just keep the old rendering.
            m_inner_props = ci_old->m_inner_props;
            m_children = ci_old->m_children;
            m_render   = ci_old->m_render;
            m_id       = ci_old->m_id;
            m_has_rendered = true;
            m_reconcile_count = ci_old->m_reconcile_count + 1;
            lean_assert(m_route == ci_old->m_route);
        } else {
            // the props have changed, so we need to rerender this component.
            m_inner_props = p_new;
            render();
        }
    } else {
        // The old component is completely different, so render as a fresh component.
        initialize();
        render();
    }
}

void component_instance::initialize() {
    vm_obj p = m_props.to_vm_obj();
    for (auto h : m_hooks) {
        h.initialize(p);
        p = h.get_props(p);
    }
    m_inner_props = p;
}


json component_instance::to_json(list<unsigned> const & route) {
    if (!m_has_rendered) {
        initialize();
        render();
    }
    json children = json::array();
    for (vdom const & x : m_render) {
        children.push_back(x.to_json(cons(m_id, route)));
    }
    json result;
    result["c"] = children;
    for (auto h : m_hooks) {
        with_mouse_capture_hook * mh = dynamic_cast<with_mouse_capture_hook *>(&h);
        if (mh) {
            result["mouse_capture"]["r"] = route_to_json(route);
            break;
        }
    }
    result["id"] = m_id;
    return result;
}

optional<vm_obj> component_instance::handle_action(vm_obj const & action) {
        optional<vm_obj> result = optional<vm_obj>(action);
        for (unsigned i = m_hooks.size() - 1; i >= 0; i--) {
            if (!result) {break;}
            result = m_hooks[i].action(*result);
        }
        return result;
}

optional<vm_obj> component_instance::handle_event(list<unsigned> const & route, unsigned handler_id, vm_obj const & event_args) {
    if (empty(route)) {
        vm_obj handler = m_handlers[handler_id].to_vm_obj();
        vm_obj action = (invoke(handler, event_args));
        return handle_action(action);
    }
    for (auto const & c : m_children) {
        if (c->m_id == head(route)) {
            optional<vm_obj> a = c->handle_event(tail(route), handler_id, event_args);
            if (a) {
                return handle_action(*a);
            } else {
                return optional<vm_obj>();
            }
        }
    }
    // given component no longer exists. This happens if the ui is updated but there are events from an old vdom
    throw invalid_handler();
}

void component_instance::handle_task_completed(list<unsigned> const & route) {
    if (empty(route)) {
        initialize();
        render();
        return;
    } else {
        for (auto const & c : m_children) {
            if (c->m_id == head(route)) {
                return c->handle_task_completed(tail(route));
            }
        }
    }
}

void component_instance::update_capture_state(unsigned ms) {
    bool should_update = false;
    for (auto h : m_hooks) {
        with_mouse_capture_hook * mh = dynamic_cast<with_mouse_capture_hook *>(&h);
        if (mh) {
            should_update |= mh->set_state(mouse_capture_state(ms));
        }
    }
    if (should_update) {
        initialize();
        render();
    }
}

void component_instance::handle_mouse_gain_capture(list<unsigned> const & route) {
    if (empty(route)) {
        // go through the hooks and find
        update_capture_state(mouse_capture_state::inside_immediate);
        return;
    } else {
        update_capture_state(mouse_capture_state::inside_child);
        for (auto const & c : m_children) {
            if (c->m_id == head(route)) {
                c->handle_mouse_gain_capture(tail(route));
                return;
            }
        }
    }
}
void component_instance::handle_mouse_lose_capture(list<unsigned> const & route) {
    update_capture_state(mouse_capture_state::outside);
    if (empty(route)) {
        return;
    } else {
        for (auto const & c : m_children) {
            if (c->m_id == head(route)) {
                c->handle_mouse_lose_capture(tail(route));
                return;
            } else {
            }
        }
    }
}

void reconcile_children(std::vector<vdom> & new_elements, std::vector<vdom> const & olds) {
    std::vector<vdom> old_elements = olds;
    for (unsigned i = 0; i < new_elements.size(); i++) {
        // [note] you get weird behaviour if multiple things have the same key or if only some elements have keys
        // but this is also true in React so I am not too worried about it as long as it doesn't crash.
        // [todo] add a warning if keys are duplicated or only present on some objects.
        auto k = new_elements[i].key();
        if (k) {
            for (unsigned j = 0; j < old_elements.size(); j++) {
                if (old_elements[j].key() == k) {
                    vdom o = old_elements[j];
                    new_elements[i].reconcile(o);
                    old_elements.erase(old_elements.begin() + j);
                    break;
                }
            }
        } else if (old_elements.size() > 0) {
            new_elements[i].reconcile(old_elements[0]);
            old_elements.erase(old_elements.begin());
        } else {
            // continue
        }
    }
}


void render_event(std::string const & name, vm_obj const & handler, std::map<std::string, unsigned> & events, std::map<unsigned, ts_vm_obj> & handlers) {
    unsigned handler_id = g_fresh_handler_id.fetch_add(1);
    events[name] = handler_id;
    handlers[handler_id] = handler;
}

vdom render_element(vm_obj const & elt, std::vector<component_instance*> & components, std::map<unsigned, ts_vm_obj> & handlers, list<unsigned> const & route) {
    // | element      {α : Type} (tag : string) (attrs : list (attr α)) (children : list (html α)) : html α
    std::string tag = to_string(cfield(elt, 0));
    vm_obj v_attrs = cfield(elt, 1);
    vm_obj v_children = cfield(elt, 2);
    json attributes;
    std::map<std::string, unsigned> events;
    optional<vdom> tooltip;
    while (!is_simple(v_attrs)) {
        vm_obj attr = head(v_attrs);
        v_attrs = tail(v_attrs);
        switch (cidx(attr)) {
            case attr_idx::val: { // val {\a} : string -> string -> attr
                std::string key = to_string(cfield(attr, 0));
                std::string value = to_string(cfield(attr, 1));
                // [note] className fields should be merged.
                if (key == "className" && attributes.find(key) != attributes.end()) {
                    std::string cn = attributes[key];
                    cn += " ";
                    cn += value;
                    attributes[key] = cn;
                } else {
                    attributes[key] = value;
                }
                break;
            } case attr_idx::mouse_event: {// on_mouse_event {\a} : mouse_event_kind -> (unit -> Action) -> html.attr
                int mouse_event_kind = cidx(cfield(attr, 0));
                vm_obj handler = cfield(attr, 1);
                switch (mouse_event_kind) {
                    case 0: render_event("onClick",      handler, events, handlers); break;
                    case 1: render_event("onMouseEnter", handler, events, handlers); break;
                    case 2: render_event("onMouseLeave", handler, events, handlers); break;
                    default: lean_unreachable();
                }
                break;
            } case attr_idx::style: { // style {a} : list (string × string) → html.attr
                auto l = cfield(attr, 0);
                while (!is_simple(l)) {
                    auto h = head(l);
                    auto k = to_string(cfield(h, 0));
                    auto v = to_string(cfield(h, 1));
                    attributes["style"][k] = v;
                    l = tail(l);
                }
                break;
            } case attr_idx::tooltip: { // tooltip {a} :  html Action → html.attr
                auto content = cfield(attr, 0);
                vdom tooltip_child = render_html(content, components, handlers, route);
                tooltip = optional<vdom>(tooltip_child);
                break;
            } case attr_idx::text_change_event: { // text_change_event {a} : (string -> Action) -> html.attr
                auto handler = cfield(attr, 0);
                render_event("onChange", handler, events, handlers);
                break;
            } default : {
                lean_unreachable();
                break;
            }
        }
    }
    std::vector<vdom> children = render_html_list(v_children, components, handlers, route);
    return vdom(new vdom_element(tag, attributes, events, children, tooltip));
}

vdom render_html(vm_obj const & html, std::vector<component_instance*> & components, std::map<unsigned, ts_vm_obj> & handlers, list<unsigned> const & route) {
    switch (cidx(html)) {
        case html_idx::element: { // | of_element {α : Type} (tag : string) (attrs : list (attr α)) (children : list (html α)) : html α
            vdom elt = render_element(html, components, handlers, route);
            return elt;
        } case html_idx::of_string: { // | of_string    {α : Type} : string → html α
            return vdom(new vdom_string(to_string(cfield(html, 0))));
        } case html_idx::of_component: { // | of_component {α : Type} {Props : Type} : Props → component Props α → html α
            vm_obj props = cfield(html, 0);
            vm_obj comp  = cfield(html, 1);
            component_instance * c = new component_instance(comp, props, route);
            components.push_back(c);
            return vdom(c);
        } default: {
            lean_unreachable();
        }
    }
}

std::vector<vdom> render_html_list(vm_obj const & htmls, std::vector<component_instance*> & components, std::map<unsigned, ts_vm_obj> & handlers, list<unsigned> const & route) {
    std::vector<vdom> elements;
    vm_obj l = htmls;
    while (!is_simple(l)) {
        vdom x = render_html(head(l), components, handlers, route);
        elements.push_back(x);
        l = tail(l);
    }
    return elements;
}

void initialize_widget() {}

void finalize_widget() {}


static pending_tasks * g_pending_tasks = nullptr;
void set_pending_tasks(pending_tasks * q) {
    if (g_pending_tasks) throw exception("cannot set task queue twice");
    g_pending_tasks = q;
}
void unset_pending_tasks() {
    g_pending_tasks = nullptr;
}
pending_tasks & get_pending_tasks() {
    return *g_pending_tasks;
}



}