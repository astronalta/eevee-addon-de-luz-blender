# Example of a custom widget that defines it's own geometry.
#
# Usage: Select a lamp in the 3D view and drag the arrow at it's rear
# to change it's energy value.
#
import bpy
from bpy.types import (
    Manipulator,
    ManipulatorGroup,
)

# Coordinates (each one is a triangle).
custom_shape_verts = (
    (3.0, 1.0, -1.0), (2.0, 2.0, -1.0), (3.0, 3.0, -1.0),
    (1.0, 3.0, 1.0), (3.0, 3.0, -1.0), (1.0, 3.0, -1.0),
    (3.0, 3.0, 1.0), (3.0, 1.0, -1.0), (3.0, 3.0, -1.0),
    (2.0, 0.0, 1.0), (3.0, 1.0, -1.0), (3.0, 1.0, 1.0),
    (2.0, 0.0, -1.0), (2.0, 2.0, 1.0), (2.0, 2.0, -1.0),
    (2.0, 2.0, -1.0), (0.0, 2.0, 1.0), (0.0, 2.0, -1.0),
    (1.0, 3.0, 1.0), (2.0, 2.0, 1.0), (3.0, 3.0, 1.0),
    (0.0, 2.0, -1.0), (1.0, 3.0, 1.0), (1.0, 3.0, -1.0),
    (2.0, 2.0, 1.0), (3.0, 1.0, 1.0), (3.0, 3.0, 1.0),
    (2.0, 2.0, -1.0), (1.0, 3.0, -1.0), (3.0, 3.0, -1.0),
    (-3.0, -1.0, -1.0), (-2.0, -2.0, -1.0), (-3.0, -3.0, -1.0),
    (-1.0, -3.0, 1.0), (-3.0, -3.0, -1.0), (-1.0, -3.0, -1.0),
    (-3.0, -3.0, 1.0), (-3.0, -1.0, -1.0), (-3.0, -3.0, -1.0),
    (-2.0, 0.0, 1.0), (-3.0, -1.0, -1.0), (-3.0, -1.0, 1.0),
    (-2.0, 0.0, -1.0), (-2.0, -2.0, 1.0), (-2.0, -2.0, -1.0),
    (-2.0, -2.0, -1.0), (0.0, -2.0, 1.0), (0.0, -2.0, -1.0),
    (-1.0, -3.0, 1.0), (-2.0, -2.0, 1.0), (-3.0, -3.0, 1.0),
    (0.0, -2.0, -1.0), (-1.0, -3.0, 1.0), (-1.0, -3.0, -1.0),
    (-2.0, -2.0, 1.0), (-3.0, -1.0, 1.0), (-3.0, -3.0, 1.0),
    (-2.0, -2.0, -1.0), (-1.0, -3.0, -1.0), (-3.0, -3.0, -1.0),
    (1.0, -1.0, 0.0), (-1.0, -1.0, 0.0), (0.0, 0.0, -5.0),
    (-1.0, -1.0, 0.0), (1.0, -1.0, 0.0), (0.0, 0.0, 5.0),
    (1.0, -1.0, 0.0), (1.0, 1.0, 0.0), (0.0, 0.0, 5.0),
    (1.0, 1.0, 0.0), (-1.0, 1.0, 0.0), (0.0, 0.0, 5.0),
    (-1.0, 1.0, 0.0), (-1.0, -1.0, 0.0), (0.0, 0.0, 5.0),
    (-1.0, -1.0, 0.0), (-1.0, 1.0, 0.0), (0.0, 0.0, -5.0),
    (-1.0, 1.0, 0.0), (1.0, 1.0, 0.0), (0.0, 0.0, -5.0),
    (1.0, 1.0, 0.0), (1.0, -1.0, 0.0), (0.0, 0.0, -5.0),
    (3.0, 1.0, -1.0), (2.0, 0.0, -1.0), (2.0, 2.0, -1.0),
    (1.0, 3.0, 1.0), (3.0, 3.0, 1.0), (3.0, 3.0, -1.0),
    (3.0, 3.0, 1.0), (3.0, 1.0, 1.0), (3.0, 1.0, -1.0),
    (2.0, 0.0, 1.0), (2.0, 0.0, -1.0), (3.0, 1.0, -1.0),
    (2.0, 0.0, -1.0), (2.0, 0.0, 1.0), (2.0, 2.0, 1.0),
    (2.0, 2.0, -1.0), (2.0, 2.0, 1.0), (0.0, 2.0, 1.0),
    (1.0, 3.0, 1.0), (0.0, 2.0, 1.0), (2.0, 2.0, 1.0),
    (0.0, 2.0, -1.0), (0.0, 2.0, 1.0), (1.0, 3.0, 1.0),
    (2.0, 2.0, 1.0), (2.0, 0.0, 1.0), (3.0, 1.0, 1.0),
    (2.0, 2.0, -1.0), (0.0, 2.0, -1.0), (1.0, 3.0, -1.0),
    (-3.0, -1.0, -1.0), (-2.0, 0.0, -1.0), (-2.0, -2.0, -1.0),
    (-1.0, -3.0, 1.0), (-3.0, -3.0, 1.0), (-3.0, -3.0, -1.0),
    (-3.0, -3.0, 1.0), (-3.0, -1.0, 1.0), (-3.0, -1.0, -1.0),
    (-2.0, 0.0, 1.0), (-2.0, 0.0, -1.0), (-3.0, -1.0, -1.0),
    (-2.0, 0.0, -1.0), (-2.0, 0.0, 1.0), (-2.0, -2.0, 1.0),
    (-2.0, -2.0, -1.0), (-2.0, -2.0, 1.0), (0.0, -2.0, 1.0),
    (-1.0, -3.0, 1.0), (0.0, -2.0, 1.0), (-2.0, -2.0, 1.0),
    (0.0, -2.0, -1.0), (0.0, -2.0, 1.0), (-1.0, -3.0, 1.0),
    (-2.0, -2.0, 1.0), (-2.0, 0.0, 1.0), (-3.0, -1.0, 1.0),
    (-2.0, -2.0, -1.0), (0.0, -2.0, -1.0), (-1.0, -3.0, -1.0),
)


class MyCustomShapeWidget(Manipulator):
    bl_idname = "VIEW3D_WT_auto_facemap"
    bl_target_properties = (
        {"id": "offset", "type": 'FLOAT', "array_length": 1},
    )

    __slots__ = (
        "custom_shape",
        "init_mouse_y",
        "init_value",
    )

    def _update_offset_matrix(self):
        # offset behind the lamp
        self.matrix_offset.col[3][2] = self.target_get_value("offset") / -10.0

    def draw(self, context):
        self._update_offset_matrix()
        self.draw_custom_shape(self.custom_shape)

    def draw_select(self, context, select_id):
        self._update_offset_matrix()
        self.draw_custom_shape(self.custom_shape, select_id=select_id)

    def setup(self):
        if not hasattr(self, "custom_shape"):
            self.custom_shape = self.new_custom_shape('TRIS', custom_shape_verts)

    def invoke(self, context, event):
        self.init_mouse_y = event.mouse_y
        self.init_value = self.target_get_value("offset")
        return {'RUNNING_MODAL'}

    def exit(self, context, cancel):
        context.area.header_text_set()
        if cancel:
            self.target_set_value("offset", self.init_value)

    def modal(self, context, event, tweak):
        delta = (event.mouse_y - self.init_mouse_y) / 10.0
        if 'SNAP' in tweak:
            delta = round(delta)
        if 'PRECISE' in tweak:
            delta /= 10.0
        value = self.init_value + delta
        self.target_set_value("offset", value)
        context.area.header_text_set("My Manipulator: %.4f" % value)
        return {'RUNNING_MODAL'}


class MyCustomShapeWidgetGroup(ManipulatorGroup):
    bl_idname = "OBJECT_WGT_lamp_test"
    bl_label = "Test Lamp Widget"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'WINDOW'
    bl_options = {'3D', 'PERSISTENT'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        return (ob and ob.type == 'LAMP')

    def setup(self, context):
        # Assign the 'offset' target property to the lamp energy.
        ob = context.object
        mpr = self.manipulators.new(MyCustomShapeWidget.bl_idname)
        mpr.target_set_prop("offset", ob.data, "energy")
        mpr.matrix_basis = ob.matrix_world.normalized()

        mpr.color = 1.0, 0.5, 1.0
        mpr.alpha = 0.5

        mpr.color_highlight = 1.0, 0.5, 1.0
        mpr.alpha_highlight = 0.5

        # units are large, so shrink to something more reasonable.
        mpr.scale_basis = 0.1
        mpr.use_draw_modal = True

        self.energy_widget = mpr

    def refresh(self, context):
        ob = context.object
        mpr = self.energy_widget
        mpr.matrix_basis = ob.matrix_world.normalized()


classes = (
    MyCustomShapeWidget,
    MyCustomShapeWidgetGroup,
)

for cls in classes:
    bpy.utils.register_class(cls)
