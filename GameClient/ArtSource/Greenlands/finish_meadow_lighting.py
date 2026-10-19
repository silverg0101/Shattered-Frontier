"""Saved preview-only daylight, applied after the material/foliage finishers."""
from dataclasses import dataclass
from pathlib import Path
import json,unreal as u
ROOT=Path(__file__).resolve().parent
@dataclass(frozen=True)
class Presentation:
    sun_intensity:float
    sun_pitch_degrees:float
    sun_yaw_degrees:float
    sun_temperature_kelvin:float
    sun_source_angle_degrees:float
    sky_intensity:float
s=Presentation(**json.loads((ROOT/'presentation-settings.json').read_text()))
assert 1<=s.sun_intensity<=20 and -80<=s.sun_pitch_degrees<=-10 and -180<=s.sun_yaw_degrees<=180
assert 4500<=s.sun_temperature_kelvin<=7500 and .5<=s.sky_intensity<=8 and .1<=s.sun_source_angle_degrees<=5
levels=u.get_editor_subsystem(u.LevelEditorSubsystem);levels.load_level('/Game/Frontier/Maps/Test/Greenlands_ArtReview')
for actor in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
    if isinstance(actor,u.DirectionalLight):
        light=actor.light_component;light.set_editor_property('intensity',s.sun_intensity);light.set_editor_property('use_temperature',True);light.set_editor_property('temperature',s.sun_temperature_kelvin);light.set_editor_property('light_source_angle',s.sun_source_angle_degrees)
        actor.set_actor_rotation(u.Rotator(pitch=s.sun_pitch_degrees,yaw=s.sun_yaw_degrees),False)
    elif isinstance(actor,u.SkyLight):actor.light_component.set_editor_property('intensity',s.sky_intensity);actor.light_component.set_editor_property('real_time_capture',True)
assert levels.save_current_level()
