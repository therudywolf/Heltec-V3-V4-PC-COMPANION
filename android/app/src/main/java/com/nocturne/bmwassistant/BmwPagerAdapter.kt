package com.nocturne.bmwassistant

import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentActivity
import androidx.viewpager2.adapter.FragmentStateAdapter

class BmwPagerAdapter(activity: FragmentActivity) : FragmentStateAdapter(activity) {
    override fun getItemCount(): Int = 5

    override fun createFragment(position: Int): Fragment = when (position) {
        0 -> DashboardFragment()
        1 -> CommandsFragment()
        2 -> MediaFragment()
        3 -> ClusterFragment()
        else -> SettingsFragment()
    }
}
