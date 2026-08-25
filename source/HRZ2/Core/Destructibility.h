#pragma once

#include "EntityComponent.h"
#include "PhysicsConstraintListener.h"

namespace HRZ2
{
	class Destructibility : public EntityComponent, public PhysicsConstraintListener
	{
	public:
		virtual const RTTI *GetRTTI() const override; // 0
		virtual ~Destructibility() override;		  // 1

		char _pad0[0x18];
		bool m_Invulnerable;						  // 0x70
		bool m_DieAtZeroHealth;						  // 0x71
		char _pad1[0x2];
		float m_Health;								  // 0x74
	};
	assert_offset(Destructibility, m_Invulnerable, 0x70);
	assert_offset(Destructibility, m_Health, 0x74);
}
