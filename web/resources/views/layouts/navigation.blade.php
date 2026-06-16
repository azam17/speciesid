<nav x-data="{ open: false }" class="sticky top-0 z-30 border-b border-slate-900/10 bg-[#f8faf8]/95 backdrop-blur">
    <div class="mx-auto max-w-[1480px] px-4 sm:px-6 lg:px-8">
        <div class="flex h-16 items-center justify-between gap-4">
            <div class="flex min-w-0 items-center gap-5">
                <a href="{{ route('dashboard') }}" class="flex shrink-0 items-center">
                    <x-application-logo class="h-9 w-auto" />
                </a>

                <div class="hidden items-center rounded-full border border-slate-900/10 bg-white px-1 py-1 shadow-sm lg:flex">
                    <x-nav-link :href="route('dashboard')" :active="request()->routeIs('dashboard')">{{ __('Dashboard') }}</x-nav-link>
                    <x-nav-link :href="route('upload.create')" :active="request()->routeIs('upload.*')">{{ __('Upload') }}</x-nav-link>
                    <x-nav-link :href="route('runs.index')" :active="request()->routeIs('runs.*') || request()->routeIs('samples.*') || request()->routeIs('reports.*')">{{ __('Runs') }}</x-nav-link>
                    <x-nav-link :href="route('databases.index')" :active="request()->routeIs('databases.*')">{{ __('Databases') }}</x-nav-link>
                    <x-nav-link :href="route('calibrations.index')" :active="request()->routeIs('calibrations.*')">{{ __('Calibrations') }}</x-nav-link>
                </div>
            </div>

            <div class="hidden items-center gap-2 lg:flex">
                <a href="{{ route('upload.create') }}" class="rounded-full bg-slate-950 px-5 py-2.5 text-sm font-bold text-white shadow-sm hover:bg-brand-800">
                    New run
                </a>
                <x-dropdown align="right" width="48">
                    <x-slot name="trigger">
                        <button class="inline-flex items-center gap-2 rounded-full border border-slate-900/10 bg-white px-4 py-2.5 text-sm font-bold text-slate-700 shadow-sm hover:border-brand-300 hover:text-slate-950 focus:outline-none focus:ring-2 focus:ring-brand-500 focus:ring-offset-2">
                            <span class="max-w-36 truncate">{{ Auth::user()->name }}</span>
                            <svg class="h-4 w-4" viewBox="0 0 20 20" fill="currentColor" aria-hidden="true">
                                <path fill-rule="evenodd" d="M5.23 7.21a.75.75 0 011.06.02L10 11.17l3.71-3.94a.75.75 0 111.08 1.04l-4.25 4.5a.75.75 0 01-1.08 0l-4.25-4.5a.75.75 0 01.02-1.06z" clip-rule="evenodd" />
                            </svg>
                        </button>
                    </x-slot>

                    <x-slot name="content">
                        <x-dropdown-link :href="route('profile.edit')">{{ __('Profile') }}</x-dropdown-link>
                        <form method="POST" action="{{ route('logout') }}">
                            @csrf
                            <x-dropdown-link :href="route('logout')" onclick="event.preventDefault(); this.closest('form').submit();">{{ __('Log Out') }}</x-dropdown-link>
                        </form>
                    </x-slot>
                </x-dropdown>
            </div>

            <div class="-me-2 flex items-center lg:hidden">
                <button @click="open = ! open" class="inline-flex h-10 w-10 items-center justify-center rounded-full border border-slate-900/10 bg-white text-slate-600 shadow-sm hover:text-slate-950 focus:outline-none focus:ring-2 focus:ring-brand-500">
                    <svg class="h-5 w-5" stroke="currentColor" fill="none" viewBox="0 0 24 24" aria-hidden="true">
                        <path :class="{'hidden': open, 'inline-flex': ! open }" class="inline-flex" stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M4 7h16M4 12h16M4 17h16" />
                        <path :class="{'hidden': ! open, 'inline-flex': open }" class="hidden" stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M6 18L18 6M6 6l12 12" />
                    </svg>
                </button>
            </div>
        </div>
    </div>

    <div :class="{'block': open, 'hidden': ! open}" class="hidden border-t border-slate-900/10 bg-white lg:hidden">
        <div class="space-y-1 pb-3 pt-2">
            <x-responsive-nav-link :href="route('dashboard')" :active="request()->routeIs('dashboard')">{{ __('Dashboard') }}</x-responsive-nav-link>
            <x-responsive-nav-link :href="route('upload.create')" :active="request()->routeIs('upload.*')">{{ __('Upload') }}</x-responsive-nav-link>
            <x-responsive-nav-link :href="route('runs.index')" :active="request()->routeIs('runs.*') || request()->routeIs('samples.*') || request()->routeIs('reports.*')">{{ __('Runs') }}</x-responsive-nav-link>
            <x-responsive-nav-link :href="route('databases.index')" :active="request()->routeIs('databases.*')">{{ __('Databases') }}</x-responsive-nav-link>
            <x-responsive-nav-link :href="route('calibrations.index')" :active="request()->routeIs('calibrations.*')">{{ __('Calibrations') }}</x-responsive-nav-link>
        </div>

        <div class="border-t border-slate-200 pb-3 pt-4">
            <div class="px-4">
                <div class="text-base font-bold text-slate-900">{{ Auth::user()->name }}</div>
                <div class="text-sm font-medium text-slate-500">{{ Auth::user()->email }}</div>
            </div>
            <div class="mt-3 space-y-1">
                <x-responsive-nav-link :href="route('profile.edit')">{{ __('Profile') }}</x-responsive-nav-link>
                <form method="POST" action="{{ route('logout') }}">
                    @csrf
                    <x-responsive-nav-link :href="route('logout')" onclick="event.preventDefault(); this.closest('form').submit();">{{ __('Log Out') }}</x-responsive-nav-link>
                </form>
            </div>
        </div>
    </div>
</nav>
