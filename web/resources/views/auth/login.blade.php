<x-guest-layout>
    @section('title', 'Log in | SpeciesID')

    <div class="mb-8 border-b border-slate-900/10 pb-6">
        <p class="text-sm font-bold uppercase tracking-widest text-brand-700">SpeciesID access</p>
        <h1 class="mt-3 text-3xl font-extrabold tracking-normal text-slate-950">Sign in to check meat samples</h1>
        <p class="mt-3 text-sm leading-6 text-slate-600">After the lab prepares a meat sample and sequencing files, upload those files to review the screening result and prepare a report.</p>
    </div>

    <x-auth-session-status class="mb-4" :status="session('status')" />

    <form method="POST" action="{{ route('login') }}" class="space-y-5">
        @csrf

        <div>
            <x-input-label for="email" :value="__('Email')" class="text-slate-700" />
            <x-text-input id="email" class="mt-2 block w-full rounded-xl border-slate-300 bg-slate-50 shadow-sm focus:border-brand-500 focus:bg-white focus:ring-brand-500" type="email" name="email" :value="old('email')" required autofocus autocomplete="username" />
            <x-input-error :messages="$errors->get('email')" class="mt-2" />
        </div>

        <div>
            <div class="flex items-center justify-between">
                <x-input-label for="password" :value="__('Password')" class="text-slate-700" />
                @if (Route::has('password.request'))
                    <a class="text-sm font-semibold text-brand-700 hover:text-brand-800" href="{{ route('password.request') }}">
                        {{ __('Forgot password?') }}
                    </a>
                @endif
            </div>
            <x-text-input id="password" class="mt-2 block w-full rounded-xl border-slate-300 bg-slate-50 shadow-sm focus:border-brand-500 focus:bg-white focus:ring-brand-500" type="password" name="password" required autocomplete="current-password" />
            <x-input-error :messages="$errors->get('password')" class="mt-2" />
        </div>

        <div class="flex items-center justify-between">
            <label for="remember_me" class="inline-flex items-center">
                <input id="remember_me" type="checkbox" class="rounded border-slate-300 text-brand-700 shadow-sm focus:ring-brand-500" name="remember">
                <span class="ms-2 text-sm text-slate-600">{{ __('Remember me') }}</span>
            </label>
        </div>

        <button type="submit" class="inline-flex w-full items-center justify-center rounded-full bg-slate-950 px-4 py-3 text-sm font-bold text-white shadow-sm hover:bg-brand-800 focus:outline-none focus:ring-2 focus:ring-brand-500 focus:ring-offset-2">
            {{ __('Log in') }}
        </button>
    </form>

    @if (Route::has('register'))
        <p class="mt-6 text-center text-sm text-slate-600">
            New lab account?
            <a href="{{ route('register') }}" class="font-semibold text-brand-700 hover:text-brand-800">Create an account</a>
        </p>
    @endif
</x-guest-layout>
