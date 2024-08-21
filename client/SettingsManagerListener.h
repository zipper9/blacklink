#ifndef SETTINGS_MANAGER_LISTENER_H_
#define SETTINGS_MANAGER_LISTENER_H_

class SimpleXML;

class SettingsManagerListener
{
	public:
		virtual ~SettingsManagerListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};

		typedef X<0> Load;
		typedef X<1> Save;
		typedef X<2> ApplySettings;

		virtual void on(Load, SimpleXML&) { }
		virtual void on(Save, SimpleXML&) { }
		virtual void on(ApplySettings) { }
};

#endif // SETTINGS_MANAGER_LISTENER_H_
