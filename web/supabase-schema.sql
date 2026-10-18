-- ============================================================================
-- Lab Developer Community - Supabase PostgreSQL Schema & Security Policies
-- Run this script in your Supabase Project -> SQL Editor
-- ============================================================================

-- 1. Create wiki_articles table
CREATE TABLE IF NOT EXISTS public.wiki_articles (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    slug TEXT NOT NULL UNIQUE,
    title TEXT NOT NULL,
    wikitext TEXT NOT NULL,
    updated_by UUID REFERENCES auth.users(id) ON DELETE SET NULL,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT timezone('utc'::text, now()) NOT NULL
);

-- 2. Create index on slug for fast lookups
CREATE INDEX IF NOT EXISTS idx_wiki_articles_slug ON public.wiki_articles(slug);

-- 3. Enable PostgreSQL Row Level Security (RLS)
ALTER TABLE public.wiki_articles ENABLE ROW LEVEL SECURITY;

-- 4. Security Policy: Anyone (authenticated or public) can READ articles
DROP POLICY IF EXISTS "Public can view wiki articles" ON public.wiki_articles;
CREATE POLICY "Public can view wiki articles"
ON public.wiki_articles
FOR SELECT
TO public
USING (true);

-- 5. Security Policy: Only verified ADMIN, DEV, or MODERATOR roles can INSERT/UPDATE
DROP POLICY IF EXISTS "Privileged roles can modify wiki articles" ON public.wiki_articles;
CREATE POLICY "Privileged roles can modify wiki articles"
ON public.wiki_articles
FOR ALL
TO authenticated
USING (
    (auth.jwt() -> 'app_metadata' ->> 'role') IN ('admin', 'dev', 'moderator')
    OR (auth.jwt() -> 'user_metadata' ->> 'role') IN ('admin', 'dev', 'moderator')
)
WITH CHECK (
    (auth.jwt() -> 'app_metadata' ->> 'role') IN ('admin', 'dev', 'moderator')
    OR (auth.jwt() -> 'user_metadata' ->> 'role') IN ('admin', 'dev', 'moderator')
);

-- ============================================================================
-- HELPER: How to promote a user to admin or dev in Supabase SQL Editor:
--
-- UPDATE auth.users
-- SET raw_app_meta_data = raw_app_meta_data || '{"role": "admin"}'::jsonb
-- WHERE email = 'your-email@example.com';
-- ============================================================================
